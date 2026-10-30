#!/bin/bash
# CIS 415 Lab 5 checker - Signal processing in C
#
# Usage: ./checker.sh [lab_dir] [n1 n2 ...]
#   lab_dir : directory containing lab5.c, Makefile, iobound (default: .)
#   n...    : process counts to test (default: 5; the lab requires >= 5, keep <= 10)
#   KEEP=1 ./checker.sh ... keeps the temp work dir (outputs, state samples) for debugging
#
# The lab is built and run inside a temp copy, so nothing in lab_dir is modified.
# The submitted iobound binary is only sanity-checked; the run tests use a
# reference iobound built from the embedded source below, so a broken iobound
# cannot hang or fork-bomb the checker.
#
# What is checked for each run of ./lab5 n:
#   - every child prints "Waiting for SIGUSR1" and blocks in sigwait (no exec yet)
#   - the parent sends SIGUSR1, SIGSTOP, SIGCONT, SIGINT to every child, in that
#     order, with >= 3 seconds of sleep before each round
#   - children exec iobound only after SIGUSR1, are stopped (state T) after
#     SIGSTOP, resume after SIGCONT, and are gone after SIGINT

LAB_DIR="$(cd "${1:-.}" && pwd)" || exit 1
shift
COUNTS=("$@")
[ ${#COUNTS[@]} -eq 0 ] && COUNTS=(5)
MIN_GAP=2.5          # minimum seconds between signal rounds (lab says sleep(3))

PASS=0
FAIL=0
pass() { echo "  [PASS] $1"; PASS=$((PASS + 1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL + 1)); }
info() { echo "  [INFO] $1"; }

for n in "${COUNTS[@]}"; do
    if ! [[ "$n" =~ ^[1-9][0-9]*$ ]] || [ "$n" -gt 10 ]; then
        echo "Invalid process count '$n' (must be 1..10)"; exit 1
    fi
    [ "$n" -lt 5 ] && echo "Note: the lab requires at least 5 children; testing $n anyway."
done

WORK="$(mktemp -d)"
ALL_KIDS="$WORK/all_kids"
cleanup() {
    pkill -9 -P $$ 2>/dev/null
    [ -f "$ALL_KIDS" ] && xargs -r kill -9 < "$ALL_KIDS" 2>/dev/null
    [ -n "$KEEP" ] && echo "kept $WORK" || rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

# ---------------------------------------------------------------- setup
echo "== Setup"
for f in lab5.c Makefile iobound; do
    if [ -f "$LAB_DIR/$f" ]; then
        cp "$LAB_DIR/$f" "$WORK/"
    else
        fail "missing $f in $LAB_DIR"
    fi
done
[ $FAIL -gt 0 ] && { echo "Cannot continue."; exit 1; }
[ -f "$LAB_DIR/signal.c" ] && cp "$LAB_DIR/signal.c" "$WORK/"
cd "$WORK" || exit 1

# Reference iobound (same as the lab4 iobound.c): busy-writes to /dev/null
# for -seconds of CPU time, printing a line at start and at finish.
mkdir -p ref
cat > ref/iobound.c <<'EOF'
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
int main(int argc, char **argv) {
    int i, j, condition = 1, seconds = 5;
    clock_t now, start;
    FILE *outfile = fopen("/dev/null", "w");
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-seconds") == 0 && i + 1 < argc) { seconds = atoi(argv[i + 1]); break; }
        fprintf(stderr, "Illegal flag: `%s'\n", argv[i]); exit(1);
    }
    printf("Process: %d - Begining to write to file.\n", getpid());
    start = clock();
    while (condition) {
        for (j = 0; j < 100; j++) fprintf(outfile, "A string! ");
        fprintf(outfile, "\n");
        now = clock();
        if ((now - start) / (double)CLOCKS_PER_SEC >= seconds) condition = 0;
    }
    fclose(outfile);
    printf("Process: %d - Finished.\n", getpid());
    return 0;
}
EOF
gcc -O0 -o ref/iobound ref/iobound.c || { echo "cannot build reference iobound"; exit 1; }

# ---------------------------------------------------------------- provided iobound
echo "== Provided iobound"
chmod +x iobound
# Run it alone in an empty dir so a binary that re-execs ./iobound cannot recurse.
mkdir -p iob_test
out=$(cd iob_test && timeout 5 ../iobound -seconds 0 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -Eq '^Process: [0-9]+ - Begin+ing' \
                  && echo "$out" | grep -Eq '^Process: [0-9]+ - Finished'; then
    pass "iobound runs and prints Begining/Finished"
else
    fail "provided iobound does not behave like iobound (rc=$rc); replace it with the lab4 iobound"
    echo "$out" | head -5 | sed 's/^/         /'
fi
cp ref/iobound iobound      # run tests against the reference binary

# ---------------------------------------------------------------- build
echo "== Build"
rm -f lab5
if make lab5 > build.log 2>&1 && [ -x lab5 ]; then
    pass "make builds lab5"
else
    fail "make failed"; cat build.log; exit 1
fi
if grep -qi "warning" build.log; then
    info "compiler warnings:"; grep -i "warning" build.log | sed 's/^/         /'
fi

# ---------------------------------------------------------------- source checks
echo "== Source checks (lab5.c)"
# Strip comments so commented-out code does not count.
gcc -fpreprocessed -dD -E -P lab5.c 2>/dev/null > lab5.nocomment.c || cp lab5.c lab5.nocomment.c
src_has() { grep -Eq "$1" lab5.nocomment.c; }
for fn in fork execvp sigemptyset sigaddset sigprocmask sigwait kill sleep; do
    src_has "\\b$fn[[:space:]]*\\(" && pass "uses $fn()" || fail "$fn() not found"
done
src_has 'sigaddset[[:space:]]*\([^)]*SIGUSR1' && pass "SIGUSR1 added to the sigset" \
                                              || fail "sigaddset(..., SIGUSR1) not found"
src_has 'sigprocmask[[:space:]]*\([[:space:]]*SIG_BLOCK' && pass "SIGUSR1 blocked with SIG_BLOCK" \
                                                         || fail "sigprocmask(SIG_BLOCK, ...) not found"
src_has 'void[[:space:]]+signaler[[:space:]]*\([^)]*pid_t[^)]*,[^)]*int[^)]*,[^)]*int[^)]*\)[[:space:]]*\{' \
    && pass "defines signaler(pid_t*, int size, int signal)" \
    || fail "signaler(pid_t *pool, int size, int signal) definition not found"
calls=$(grep -Eo 'signaler[[:space:]]*\([^;]*(SIGUSR1|SIGSTOP|SIGCONT|SIGINT)[^;]*\)[[:space:]]*;' lab5.nocomment.c \
        | grep -Eo 'SIGUSR1|SIGSTOP|SIGCONT|SIGINT' | tr '\n' ' ')
if [ "$calls" == "SIGUSR1 SIGSTOP SIGCONT SIGINT " ]; then
    pass "signaler called with SIGUSR1, SIGSTOP, SIGCONT, SIGINT in order"
else
    fail "expected signaler() calls with SIGUSR1, SIGSTOP, SIGCONT, SIGINT in order (found: ${calls:-none})"
fi
src_has '\bstrsignal[[:space:]]*\(' || info "strsignal() not used (hint: print the signal name, not the number)"
if src_has '\bmalloc[[:space:]]*\(' && ! src_has '\bfree[[:space:]]*\('; then
    fail "malloc() without free()"
else
    pass "dynamic memory freed (or none used)"
fi

# ---------------------------------------------------------------- argument handling
echo "== Argument handling"
out=$(timeout 5 ./lab5 2>&1); rc=$?
if [ $rc -ne 124 ] && [ $rc -ne 0 ]; then
    pass "no arguments -> exits with an error (status $rc)"
else
    fail "no arguments should print usage and exit with non-zero status (status $rc)"
fi
[ -n "$out" ] && info "message: $(echo "$out" | head -1)"

# ---------------------------------------------------------------- helpers
# Map a printed signal (number, SIGxxx, or strsignal() text) to a short name.
sig_name() {
    case "$1" in
        10|SIGUSR1|"User defined signal 1") echo USR1 ;;
        19|SIGSTOP|"Stopped (signal)")      echo STOP ;;
        18|SIGCONT|Continued)               echo CONT ;;
        2|SIGINT|Interrupt)                 echo INT ;;
        *)                                  echo "?" ;;
    esac
}
# Print "<SIG> <child_pid> <parent_pid>" for every "Sending signal" line of a file.
parse_sends() {
    grep -Ei 'Sending signal' "$1" \
    | sed -E 's/.*[Pp]arent [Pp]rocess:?[[:space:]]*([0-9]+).*[Ss]ending signal:?[[:space:]]*(.+)[[:space:]]+to child process:?[[:space:]]*([0-9]+).*/\2|\3|\1/' \
    | while IFS='|' read -r s c p; do echo "$(sig_name "$s") $c $p"; done
}

# ---------------------------------------------------------------- run tests
run_test() {
    local n=$1
    local out="run_$n.out" tl="run_$n.timeline" samples="run_$n.samples"
    local limit=$((12 * n + 30))
    echo "== ./lab5 $n"
    : > "$out"; : > "$tl"; : > "$samples"

    # stdbuf -oL: line buffering survives fork, so lines are not duplicated
    # and arrive in the order they were printed.
    stdbuf -oL -eL ./lab5 "$n" > "$out" 2>&1 &
    local lpid=$! start now seen=0 total kids p phase stat comm last cnt nsend
    start=$(date +%s.%N)

    # Poll: timestamp new output lines, track children, sample their states.
    while :; do
        now=$(echo "$(date +%s.%N) - $start" | bc)
        total=$(wc -l < "$out")
        if [ "$total" -gt "$seen" ]; then
            sed -n "$((seen + 1)),${total}p" "$out" | sed "s/^/$now|/" >> "$tl"
            seen=$total
        fi
        kill -0 "$lpid" 2>/dev/null || break
        kids=$(pgrep -P "$lpid")
        [ -n "$kids" ] && echo "$kids" >> "$ALL_KIDS"
        # Phase = last signal round whose sends have all been printed.
        nsend=$(grep -Eic 'Sending signal' "$out")
        phase=NONE
        for s in USR1 STOP CONT INT; do
            cnt=$(parse_sends "$out" | grep -c "^$s ")
            [ "$cnt" -ge "$n" ] && phase=$s
        done
        # Sample: time, phase, then pid:state:comm for every known child.
        last="$now $phase"
        for p in $(sort -un "$ALL_KIDS" 2>/dev/null); do
            read -r stat comm < <(ps -o stat=,comm= -p "$p" 2>/dev/null)
            comm=${comm%% *}; last+=" $p:${stat:0:1}:${comm:-gone}"
        done
        # Drop the sample if a round started while it was taken (phase would be stale).
        [ "$(grep -Eic 'Sending signal' "$out")" -eq "$nsend" ] && echo "$last" >> "$samples"
        if [ "$(echo "$now > $limit" | bc)" -eq 1 ]; then
            kill -9 "$lpid" 2>/dev/null
            fail "timed out after ${limit}s"; break
        fi
        sleep 0.1
    done
    wait "$lpid" 2>/dev/null; local rc=$?
    sleep 0.5
    info "exit code $rc, wall time $(printf '%.1f' "$(echo "$(date +%s.%N) - $start" | bc)")s"

    # ---- children and their messages
    local child_pids wait_pids recv_pids nk
    child_pids=$(sort -un "$ALL_KIDS" 2>/dev/null)
    nk=$(echo -n "$child_pids" | grep -c .)
    [ "$nk" -eq "$n" ] && pass "parent forked $n children" || fail "expected $n children, observed $nk"

    wait_pids=$(grep -E 'Child [Pp]rocess:?[[:space:]]*[0-9]+[[:space:]]*-[[:space:]]*Waiting for SIGUSR1' "$out" \
                | sed -E 's/.*Child [Pp]rocess:?[[:space:]]*([0-9]+).*/\1/' | sort -un)
    if [ -n "$wait_pids" ] && [ "$wait_pids" == "$child_pids" ]; then
        pass "each child prints 'Child Process: <pid> - Waiting for SIGUSR1...'"
    else
        fail "'Child Process: <pid> - Waiting for SIGUSR1...' missing or wrong pid ($(echo -n "$wait_pids" | grep -c .)/$n)"
    fi
    recv_pids=$(grep -E 'Child [Pp]rocess:?[[:space:]]*[0-9]+[[:space:]]*-[[:space:]]*Received signal:?.*SIGUSR1.*Calling exec' "$out" \
                | sed -E 's/.*Child [Pp]rocess:?[[:space:]]*([0-9]+).*/\1/' | sort -un)
    if [ -n "$recv_pids" ] && [ "$recv_pids" == "$child_pids" ]; then
        pass "each child prints 'Child Process: <pid> - Received signal: SIGUSR1 - Calling exec().'"
    else
        fail "'Child Process: <pid> - Received signal: SIGUSR1 - Calling exec().' missing or wrong pid ($(echo -n "$recv_pids" | grep -c .)/$n)"
    fi

    # ---- signal rounds
    local sends seq expect="" s
    sends=$(parse_sends "$out")
    seq=$(echo "$sends" | awk 'NF{print $1}' | uniq -c | awk '{print $2"x"$1}' | tr '\n' ' ')
    for s in USR1 STOP CONT INT; do expect+="${s}x$n "; done
    if [ "$seq" == "$expect" ]; then
        pass "parent sends SIGUSR1, SIGSTOP, SIGCONT, SIGINT to all $n children, in order"
    else
        fail "signal sequence wrong: expected [$expect] got [${seq:-nothing}]"
    fi
    grep -Ei 'Sending signal' "$out" | grep -Eq 'Parent process:? *[0-9]+ - Sending signal:? *.+ to child process:? *[0-9]+' \
        && pass "send message format 'Parent process: <pid> - Sending signal: <signal> to child process: <pid>'" \
        || fail "send message not in format 'Parent process: <pid> - Sending signal: <signal> to child process: <pid>'"
    echo "$sends" | grep -q '^? ' && fail "unrecognized signal value in send message"
    if [ -n "$sends" ] && [ "$(echo "$sends" | awk '{print $3}' | sort -u)" == "$lpid" ]; then
        pass "send messages show the parent's pid ($lpid)"
    else
        fail "send messages do not show the parent's pid ($lpid)"
    fi
    local bad=0 targets
    for s in USR1 STOP CONT INT; do
        targets=$(echo "$sends" | awk -v s="$s" '$1==s{print $2}' | sort -n)
        [ "$targets" == "$child_pids" ] || bad=$((bad + 1))
    done
    [ $bad -eq 0 ] && pass "every round targets exactly the child pids" \
                   || fail "$bad signal round(s) do not target exactly the child pids"

    # ---- timing: >= MIN_GAP seconds before each round
    local t_prev=0 t gaps="" tbad=0 line
    for s in USR1 STOP CONT INT; do
        line=$(grep -Ein 'Sending signal' "$out" | while IFS= read -r l; do
                  txt=${l#*:}; sig=$(echo "$txt" | sed -E 's/.*[Ss]ending signal:?[[:space:]]*(.+)[[:space:]]+to child.*/\1/')
                  [ "$(sig_name "$sig")" == "$s" ] && { echo "$txt"; break; }
               done)
        t=$(grep -F -- "|$line" "$tl" | head -1 | cut -d'|' -f1)
        [ -z "$t" ] && { tbad=1; break; }
        gaps+="$(printf '%.1f' "$(echo "$t - $t_prev" | bc)")s "
        [ "$(echo "$t - $t_prev >= $MIN_GAP" | bc)" -eq 1 ] || tbad=1
        t_prev=$t
    done
    [ $tbad -eq 0 ] && pass "signaler sleeps ~3s before each round (gaps: $gaps)" \
                    || fail "signaler must sleep 3s before sending (gaps: ${gaps:-n/a})"

    # ---- ordering: exec only after SIGUSR1
    local first_usr1 first_recv first_begin
    first_usr1=$(grep -Ein 'Sending signal' "$out" | head -1 | cut -d: -f1)
    first_recv=$(grep -En 'Received signal' "$out" | head -1 | cut -d: -f1)
    first_begin=$(grep -En '^Process: [0-9]+ - Begin+ing' "$out" | head -1 | cut -d: -f1)
    if [ -n "$first_usr1" ] && [ -n "$first_recv" ] && [ "$first_recv" -gt "$first_usr1" ] \
       && { [ -z "$first_begin" ] || [ "$first_begin" -gt "$first_usr1" ]; }; then
        pass "children receive SIGUSR1 / exec only after the parent sends it"
    else
        fail "a child received the signal or exec'd before SIGUSR1 was sent"
    fi
    local begin_pids
    begin_pids=$(grep -Eo '^Process: [0-9]+ - Begin+ing' "$out" | grep -Eo '[0-9]+' | sort -n)
    [ -n "$begin_pids" ] && [ "$begin_pids" == "$child_pids" ] \
        && pass "all $n children exec'd iobound (same pid as the forked child)" \
        || fail "expected $n iobound starts from the child pids, got $(echo -n "$begin_pids" | grep -c .)"
    grep -Eq '^Process: [0-9]+ - Finished' "$out" \
        && info "$(grep -Ec '^Process: [0-9]+ - Finished' "$out") iobound finished before SIGINT (ok: iobound counts CPU time)"

    # ---- process states from samples
    # all_in PHASE STATE : number of samples in PHASE where every child is in STATE
    all_in() {
        awk -v ph="$1" -v st="$2" -v n="$n" '$2==ph && NF-2==n {
            ok=1; for (i=3;i<=NF;i++) { split($i,a,":"); if (a[2]!=st) ok=0 } ; c+=ok } END{print c+0}' "$samples"
    }
    local cnt_wait cnt_early
    cnt_wait=$(awk -v n="$n" '$2=="NONE" && NF-2==n {
            ok=1; for (i=3;i<=NF;i++) { split($i,a,":"); if (a[2]!="S" || a[3]=="iobound") ok=0 } ; c+=ok } END{print c+0}' "$samples")
    cnt_early=$(awk '$2=="NONE" { for (i=3;i<=NF;i++) { split($i,a,":"); if (a[3]=="iobound") c++ } } END{print c+0}' "$samples")
    [ "$cnt_wait" -gt 0 ] && [ "$cnt_early" -eq 0 ] \
        && pass "before SIGUSR1 all children are blocked (sleeping) and have not exec'd" \
        || fail "before SIGUSR1 children were not all blocked in sigwait (blocked samples=$cnt_wait, early exec samples=$cnt_early)"
    [ "$(all_in STOP T)" -gt 0 ] && pass "after SIGSTOP all children are stopped (state T)" \
                                 || fail "never observed all children stopped after SIGSTOP (already exited?)"
    local cnt_cont
    cnt_cont=$(awk -v n="$n" '$2=="CONT" && NF-2==n {
            ok=1; for (i=3;i<=NF;i++) { split($i,a,":"); if (a[2]=="T") ok=0 } ; c+=ok } END{print c+0}' "$samples")
    [ "$cnt_cont" -gt 0 ] && pass "after SIGCONT all children are running again" \
                          || fail "children still stopped after SIGCONT"
    local alive=0
    for p in $child_pids; do
        stat=$(ps -o stat= -p "$p" 2>/dev/null)
        [ -n "$stat" ] && [ "${stat:0:1}" != "Z" ] && alive=$((alive + 1))
    done
    [ $alive -eq 0 ] && pass "all children terminated after SIGINT" \
                     || fail "$alive child(ren) still alive after SIGINT and lab5 exit"
    [ "$rc" -eq 0 ] && pass "lab5 exits with status 0" || fail "lab5 exit status $rc"

    echo "  ---- program output (seconds since start | line) ----"
    awk '{t=$0; sub(/\|.*/, "", t); sub(/^[^|]*\|/, ""); printf "  | %5.1fs  %s\n", t, $0}' "$tl"
    # kill anything left behind before the next run
    [ -f "$ALL_KIDS" ] && xargs -r kill -9 < "$ALL_KIDS" 2>/dev/null
    rm -f "$ALL_KIDS"
}

for n in "${COUNTS[@]}"; do
    run_test "$n"
done

# ---------------------------------------------------------------- memory check
if command -v valgrind > /dev/null; then
    echo "== Valgrind (./lab5 5, parent process only)"
    valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=42 \
        --child-silent-after-fork=yes ./lab5 5 > valgrind.log 2>&1
    rc=$?
    if [ $rc -eq 42 ]; then
        fail "valgrind reports memory errors/leaks"
        grep -E "definitely lost|ERROR SUMMARY" valgrind.log | sed 's/^/         /'
    else
        pass "no definite leaks or memory errors"
    fi
else
    info "valgrind not installed; skipping memory check"
fi

# ---------------------------------------------------------------- summary
echo
echo "== Summary: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
