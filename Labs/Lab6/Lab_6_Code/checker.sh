#!/bin/bash
# CIS 415 Lab 6 checker - Linux /proc file system
#
# Usage: ./checker.sh [lab_dir]
#   lab_dir : directory containing lab6.c and Makefile (default: .)
#   KEEP=1 ./checker.sh ... keeps the temp work dir (outputs, probe logs) for debugging
#
# The lab is built and run inside a temp work dir, so lab6.c and the Makefile are
# not modified. The work dir is created inside lab_dir (removed on exit) because
# /tmp is often tmpfs, where write_bytes/read_bytes are always 0.
#
# What is checked:
#   - lab6.c builds with the Makefile and uses the functions the lab asks for
#   - read_memory(), read_time(), read_io() are called on a test process that has
#     done a known amount of memory, CPU and disk work and is then frozen with
#     SIGSTOP. Their printed values must equal what the checker itself reads
#     from /proc/<pid>/status, /proc/<pid>/stat and /proc/<pid>/io.
#   - the three functions close their files (no file descriptor leak) and do not
#     crash on a pid that does not exist
#   - ./lab6 runs end to end and prints plausible values for its child

LAB_DIR="$(cd "${1:-.}" && pwd)" || exit 1

PASS=0
FAIL=0
pass() { echo "  [PASS] $1"; PASS=$((PASS + 1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL + 1)); }
info() { echo "  [INFO] $1"; }

WORK="$(mktemp -d "$LAB_DIR/.lab6_check.XXXXXX" 2>/dev/null || mktemp -d)"
TARGETS="$WORK/targets"      # pids of test processes (started in subshells)
cleanup() {
    [ -f "$TARGETS" ] && xargs -r kill -9 < "$TARGETS" 2>/dev/null
    pkill -9 -P $$ 2>/dev/null
    [ -n "$KEEP" ] && echo "kept $WORK" || rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

# ---------------------------------------------------------------- setup
echo "== Setup"
for f in lab6.c Makefile; do
    if [ -f "$LAB_DIR/$f" ]; then
        cp "$LAB_DIR/$f" "$WORK/"
    else
        fail "missing $f in $LAB_DIR"
    fi
done
[ $FAIL -gt 0 ] && { echo "Cannot continue."; exit 1; }
cd "$WORK" || exit 1

# ---------------------------------------------------------------- build
echo "== Build"
rm -f lab6
if make lab6 > build.log 2>&1 && [ -x lab6 ]; then
    pass "make builds lab6"
else
    fail "make failed"; cat build.log; exit 1
fi
if grep -qi "warning" build.log; then
    info "compiler warnings:"; grep -i "warning" build.log | sed 's/^/         /'
fi

# ---------------------------------------------------------------- source checks
echo "== Source checks (lab6.c)"
# Strip comments so commented-out code does not count.
gcc -fpreprocessed -dD -E -P lab6.c 2>/dev/null > lab6.nocomment.c || cp lab6.c lab6.nocomment.c
src_has() { grep -Eq "$1" lab6.nocomment.c; }
for fn in snprintf fopen fgets strncmp sscanf fclose; do
    src_has "\\b$fn[[:space:]]*\\(" && pass "uses $fn()" || fail "$fn() not found"
done
src_has 'sysconf[[:space:]]*\([[:space:]]*_SC_CLK_TCK' && pass "uses sysconf(_SC_CLK_TCK)" \
                                                       || fail "sysconf(_SC_CLK_TCK) not found"
for p in status stat io; do
    src_has "\"/proc/%d/$p\"" && pass "builds the path \"/proc/%d/$p\"" \
                              || fail "path \"/proc/%d/$p\" not found (build it with snprintf and the pid)"
done
src_has '/proc/self' && fail "uses /proc/self - the functions must read /proc/<pid> of the pid argument"

# ---------------------------------------------------------------- test programs
# target: does known memory / CPU / disk work, prints "ready", then waits to be
# frozen with SIGSTOP so its /proc values stay constant while they are compared.
cat > target.c <<'EOF'
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
int main(int argc, char **argv) {
    long mb = atol(argv[1]);
    double cpu = atof(argv[2]);
    const char *file = argv[3];
    char *mem = malloc(mb << 20);
    if (!mem) return 1;
    memset(mem, 1, mb << 20);                           /* raise VmRSS */
    volatile unsigned long x = 0;
    clock_t start = clock();                            /* burn CPU time */
    while ((double)(clock() - start) / CLOCKS_PER_SEC < cpu)
        for (int i = 0; i < 100000; i++) x += i;
    static char buf[1 << 16];
    memset(buf, 'x', sizeof(buf));
    int fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644);   /* write_bytes */
    for (int i = 0; i < 4; i++) write(fd, buf, sizeof(buf));
    fsync(fd);
    close(fd);
    fd = open(file, O_RDONLY);                          /* read_bytes */
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    while (read(fd, buf, sizeof(buf)) > 0) ;
    close(fd);
    unlink(file);
    fd = open("/dev/zero", O_RDONLY);                   /* rchar >> read_bytes */
    for (int i = 0; i < 64; i++) read(fd, buf, sizeof(buf));
    close(fd);
    printf("ready %lu\n", (unsigned long)x & 1);
    fflush(stdout);
    for (;;) pause();
}
EOF

# probe: links the student's read_* functions (main renamed) and calls one of them.
#   probe memory|time|io <pid>   call one function once
#   probe leak <pid>             call all three 300 times, print "fds <before> <after>"
cat > probe.c <<'EOF'
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
void read_memory(pid_t pid);
void read_time(pid_t pid);
void read_io(pid_t pid);
static int count_fds(void) {
    int n = 0;
    DIR *d = opendir("/proc/self/fd");
    struct dirent *e;
    while ((e = readdir(d)) != NULL) n++;
    closedir(d);
    return n;
}
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    pid_t pid = (pid_t)atol(argv[2]);
    if (strcmp(argv[1], "memory") == 0) read_memory(pid);
    else if (strcmp(argv[1], "time") == 0) read_time(pid);
    else if (strcmp(argv[1], "io") == 0) read_io(pid);
    else if (strcmp(argv[1], "leak") == 0) {
        if (!freopen("/dev/null", "w", stdout)) return 2;
        int before = count_fds();
        for (int i = 0; i < 300; i++) { read_memory(pid); read_time(pid); read_io(pid); }
        fprintf(stderr, "fds %d %d\n", before, count_fds());
    } else return 2;
    fflush(stdout);
    return 0;
}
EOF

echo "== Build test programs"
gcc -O0 -o target target.c || { echo "cannot build target"; exit 1; }
if gcc -Dmain=lab6_main -c lab6.c -o lab6_probe.o > probe_build.log 2>&1 \
   && gcc -O0 -o probe probe.c lab6_probe.o >> probe_build.log 2>&1; then
    pass "read_memory(), read_time(), read_io() link with the test probe"
else
    fail "cannot link read_memory/read_time/read_io (keep their names and 'void f(pid_t pid)' signatures)"
    sed 's/^/         /' probe_build.log
    exit 1
fi

# ---------------------------------------------------------------- helpers
TICKS=$(getconf CLK_TCK)
# Expected values, read by the checker straight from /proc.
ref_status() { awk -v k="$2:" '$1==k {print $2}' "/proc/$1/status"; }
ref_io()     { awk -v k="$2:" '$1==k {print $2}' "/proc/$1/io"; }
# In /proc/<pid>/stat, field 2 (comm) may contain spaces; cut after the last ')'
# so that field 3 (state) becomes $1, utime (14) is $12 and stime (15) is $13.
ref_cpu()    { sed -E 's/.*\) //' "/proc/$1/stat" | awk -v t="$TICKS" '{printf "%.4f", ($12 + $13) / t}'; }

# run_probe <func> <pid> : run the probe, fail on crash / timeout, echo its stdout
run_probe() {
    timeout 5 ./probe "$1" "$2" > "probe_$1.out" 2> "probe_$1.err"
    local rc=$?
    if [ $rc -ne 0 ]; then
        [ $rc -eq 124 ] && echo "TIMEOUT" || echo "EXIT $rc"
        return 1
    fi
    cat "probe_$1.out"
}

# start_target <mb> <cpu_seconds> : start a target, wait for "ready", SIGSTOP it
start_target() {
    ./target "$1" "$2" "$WORK/io_$1.dat" > "target_$1.out" 2>&1 &
    local tpid=$! i
    echo "$tpid" >> "$TARGETS"
    for i in $(seq 1 200); do
        grep -q ready "target_$1.out" && break
        sleep 0.05
    done
    if ! grep -q ready "target_$1.out"; then
        echo "target did not start"; return 1
    fi
    kill -STOP "$tpid"
    # wait until it is really stopped, so utime/stime no longer change
    for i in $(seq 1 50); do
        [ "$(ps -o stat= -p "$tpid" | cut -c1)" == "T" ] && break
        sleep 0.02
    done
    echo "$tpid"
}

# ---------------------------------------------------------------- function tests
test_target() {
    local mb=$1 cpu=$2 tpid out
    echo "== Test process: ${mb} MB touched, ${cpu}s CPU, 256 KB written+read"
    tpid=$(start_target "$mb" "$cpu") || { fail "could not start the test process"; return; }
    info "test process pid $tpid (stopped)"

    # ---- read_memory
    local rss size got_rss got_size
    rss=$(ref_status "$tpid" VmRSS); size=$(ref_status "$tpid" VmSize)
    if ! out=$(run_probe memory "$tpid"); then
        fail "read_memory($tpid) crashed or hung ($out)"
    else
        line=$(echo "$out" | grep -E '^\[Memory\]')
        got_rss=$(echo "$line"  | sed -nE 's/.*VmRSS:[[:space:]]*(-?[0-9]+).*/\1/p')
        got_size=$(echo "$line" | sed -nE 's/.*VmSize:[[:space:]]*(-?[0-9]+).*/\1/p')
        if [ -z "$line" ]; then
            fail "read_memory: no '[Memory] VmRSS: <n> kB, VmSize: <n> kB' line (got: $(echo "$out" | head -1))"
        elif [ "$got_rss" == "$rss" ] && [ "$got_size" == "$size" ]; then
            pass "read_memory: VmRSS $got_rss kB, VmSize $got_size kB (match /proc/$tpid/status)"
        elif [ "$got_rss" == "$size" ] && [ "$got_size" == "$rss" ]; then
            fail "read_memory: VmRSS and VmSize are swapped (got VmRSS $got_rss, VmSize $got_size; expected VmRSS $rss, VmSize $size)"
        else
            fail "read_memory: got VmRSS ${got_rss:-?} kB, VmSize ${got_size:-?} kB; expected VmRSS $rss kB, VmSize $size kB"
        fi
    fi

    # ---- read_time
    local cpu_ref got_cpu
    cpu_ref=$(ref_cpu "$tpid")
    if ! out=$(run_probe time "$tpid"); then
        fail "read_time($tpid) crashed or hung ($out)"
    else
        line=$(echo "$out" | grep -E '^\[CPU\]')
        got_cpu=$(echo "$line" | sed -nE 's/.*CPU time:[[:space:]]*(-?[0-9.]+).*/\1/p')
        if [ -z "$line" ]; then
            fail "read_time: no '[CPU] Total CPU time: <x> sec' line (got: $(echo "$out" | head -1))"
        elif [ "$got_cpu" == "$cpu_ref" ]; then
            pass "read_time: $got_cpu sec (matches (utime + stime) / $TICKS from /proc/$tpid/stat)"
        else
            fail "read_time: got ${got_cpu:-?} sec, expected $cpu_ref sec ((utime + stime) / sysconf(_SC_CLK_TCK))"
        fi
    fi

    # ---- read_io
    local rb wb rchar wchar got_rb got_wb
    rb=$(ref_io "$tpid" read_bytes); wb=$(ref_io "$tpid" write_bytes)
    rchar=$(ref_io "$tpid" rchar);   wchar=$(ref_io "$tpid" wchar)
    if ! out=$(run_probe io "$tpid"); then
        fail "read_io($tpid) crashed or hung ($out)"
    else
        line=$(echo "$out" | grep -E '^\[IO\]')
        got_rb=$(echo "$line" | sed -nE 's/.*read_bytes:[[:space:]]*(-?[0-9]+).*/\1/p')
        got_wb=$(echo "$line" | sed -nE 's/.*write_bytes:[[:space:]]*(-?[0-9]+).*/\1/p')
        if [ -z "$line" ]; then
            fail "read_io: no '[IO] read_bytes: <n> bytes, write_bytes: <n> bytes' line (got: $(echo "$out" | head -1))"
        elif [ "$got_rb" == "$rb" ] && [ "$got_wb" == "$wb" ]; then
            pass "read_io: read_bytes $got_rb, write_bytes $got_wb (match /proc/$tpid/io)"
        elif [ "$got_rb" == "$rchar" ] || [ "$got_wb" == "$wchar" ]; then
            fail "read_io: printed rchar/wchar instead of read_bytes/write_bytes (got $got_rb / $got_wb; expected $rb / $wb)"
        elif [ "$got_rb" == "$wb" ] && [ "$got_wb" == "$rb" ]; then
            fail "read_io: read_bytes and write_bytes are swapped (got $got_rb / $got_wb; expected $rb / $wb)"
        else
            fail "read_io: got read_bytes ${got_rb:-?}, write_bytes ${got_wb:-?}; expected $rb, $wb"
        fi
        [ "$wb" == "0" ] && \
            info "this filesystem reports 0 disk I/O (tmpfs?), so read_io is only weakly tested"
    fi

    kill -9 "$tpid" 2>/dev/null
}

test_target 16 0.3
test_target 64 0.8

# ---------------------------------------------------------------- robustness
echo "== Robustness"
tpid=$(start_target 4 0.1)
if [ -n "$tpid" ]; then
    timeout 20 ./probe leak "$tpid" > /dev/null 2> probe_leak.err
    rc=$?
    read -r _ before after < <(grep '^fds' probe_leak.err)
    if [ $rc -ne 0 ]; then
        fail "calling the three functions 300 times crashed or hung (status $rc)"
    elif [ -n "$before" ] && [ "$before" == "$after" ]; then
        pass "no file descriptor leak after 300 calls of each function (files are fclose()d)"
    else
        fail "file descriptor leak: $before open fds before, $after after 300 calls (missing fclose()?)"
    fi
    kill -9 "$tpid" 2>/dev/null
fi

# a pid larger than pid_max can never exist
BADPID=$(( $(cat /proc/sys/kernel/pid_max) + 1 ))
bad=0
for fn in memory time io; do
    if ! out=$(run_probe "$fn" "$BADPID"); then
        fail "read_$fn($BADPID) on a nonexistent pid crashed or hung ($out)"; bad=1
    fi
done
[ $bad -eq 0 ] && pass "nonexistent pid: all three functions return without crashing"
[ -s probe_io.err ] || info "no error message for a nonexistent pid (hint: check fopen() and use perror())"

# ---------------------------------------------------------------- full run
echo "== ./lab6"
timeout 30 stdbuf -oL ./lab6 > run.out 2>&1
rc=$?
[ $rc -eq 0 ] && pass "lab6 exits with status 0" || fail "lab6 exit status $rc"

child=$(sed -nE 's/^Child PID: ([0-9]+).*/\1/p' run.out | head -1)
grep -Eq "^========= Reading /proc/${child:-X} =========" run.out \
    && pass "parent reads /proc of the child (pid $child)" \
    || fail "'========= Reading /proc/<child pid> =========' not found"

mem=$(grep -E '^\[Memory\]' run.out | head -1)
rss=$(echo "$mem"  | sed -nE 's/.*VmRSS:[[:space:]]*(-?[0-9]+).*/\1/p')
size=$(echo "$mem" | sed -nE 's/.*VmSize:[[:space:]]*(-?[0-9]+).*/\1/p')
if [ -n "$rss" ] && [ -n "$size" ] && [ "$rss" -gt 0 ] && [ "$size" -ge "$rss" ]; then
    pass "memory values look valid (VmRSS $rss kB <= VmSize $size kB)"
else
    fail "memory values invalid: '${mem:-no [Memory] line}'"
fi

cpu=$(grep -E '^\[CPU\]' run.out | sed -nE 's/.*CPU time:[[:space:]]*(-?[0-9.]+).*/\1/p' | head -1)
if [ -n "$cpu" ] && [ "$(echo "$cpu > 0.1" | bc)" -eq 1 ] && [ "$(echo "$cpu < 5" | bc)" -eq 1 ]; then
    pass "CPU time looks valid ($cpu sec after ~1 s of busy loop)"
else
    fail "CPU time invalid: '${cpu:-no [CPU] line}' (the child busy-loops for ~1 s before the parent reads it)"
fi

iol=$(grep -E '^\[IO\]' run.out | head -1)
rb=$(echo "$iol" | sed -nE 's/.*read_bytes:[[:space:]]*(-?[0-9]+).*/\1/p')
wb=$(echo "$iol" | sed -nE 's/.*write_bytes:[[:space:]]*(-?[0-9]+).*/\1/p')
if [ -n "$rb" ] && [ -n "$wb" ] && [ "$rb" -ge 0 ] && [ "$wb" -ge 0 ]; then
    pass "I/O values look valid (read_bytes $rb, write_bytes $wb)"
else
    fail "I/O values invalid: '${iol:-no [IO] line}'"
fi

echo "  ---- program output ----"
grep -v '^This is a test line' run.out | sed 's/^/  | /'

# ---------------------------------------------------------------- summary
echo
echo "== Summary: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
