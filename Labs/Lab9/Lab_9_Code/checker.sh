#!/bin/bash
# CIS 415 Lab 9 checker - Duck Park passengers (pthreads + mutex)
#
# Usage: ./checker.sh [lab_dir] [n1 n2 ...]
#   lab_dir : directory containing lab9.c and Makefile (default: .)
#   n...    : thread counts to test (default: 1 3 5; keep <= 10, each run takes ~n+3 seconds)
#   SRC=lab9_ai.c ./checker.sh ...   checks another source file (it is copied in as lab9.c)
#   KEEP=1 ./checker.sh ...          keeps the temp work dir (outputs) for debugging
#   NOVALGRIND=1 ./checker.sh ...    skips the valgrind check
#
# The lab is built and run inside a temp copy, so nothing in lab_dir is modified.
#
# What is checked for each run of ./lab9 n (want = min(n, MAX_THREADS) passengers):
#   - the program exits with status 0 (no deadlock) and prints
#     "- Number of passenger threads: want"
#   - every passenger 0..want-1 prints each step exactly once, in this order:
#       entered the park -> is exploring the park -> finished exploring ->
#       First Print -> Second Print (acquired ticket) -> has entered the ride queue
#     (a passenger printed twice / missing means threads share an argument, e.g. &i)
#   - every line about a passenger starts with "[Time: t]"
#   - exploring takes 1-3 s (finished - exploring is 1..4 in whole seconds)
#   - ticket booth: "First Print" is always directly followed by the "Second Print" of
#     the same passenger (both prints and the sleep are inside ticket_mutex), 1 s apart
#   - tickets are numbered #1, #2, ... in the order they are sold
#   - ride queue sizes are 1, 2, ... in the order passengers enter the queue
#   - the final line reports want passengers, want tickets sold and queue size want
#   - the run takes at least want+1 s (the booth sleeps are serialized by the lock)
#   - all want threads exist at the same time (all created before main joins)
#   - no memory leaks or errors under valgrind (if valgrind is installed)

LAB_DIR="$(cd "${1:-.}" && pwd)" || exit 1
shift
COUNTS=("$@")
[ ${#COUNTS[@]} -eq 0 ] && COUNTS=(1 3 5)
SRC="${SRC:-lab9.c}"

PASS=0
FAIL=0
pass() { echo "  [PASS] $1"; PASS=$((PASS + 1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL + 1)); }
info() { echo "  [INFO] $1"; }

for n in "${COUNTS[@]}"; do
    if ! [[ "$n" =~ ^[1-9][0-9]*$ ]] || [ "$n" -gt 10 ]; then
        echo "Invalid thread count '$n' (must be 1..10)"; exit 1
    fi
done

WORK="$(mktemp -d)"
cleanup() {
    pkill -9 -P $$ 2>/dev/null
    [ -n "$KEEP" ] && echo "kept $WORK" || rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

# ---------------------------------------------------------------- setup
echo "== Setup"
if [ -f "$LAB_DIR/$SRC" ]; then
    cp "$LAB_DIR/$SRC" "$WORK/lab9.c"
    [ "$SRC" != "lab9.c" ] && info "checking $SRC (copied in as lab9.c)"
else
    fail "missing $SRC in $LAB_DIR"
fi
if [ -f "$LAB_DIR/Makefile" ]; then
    cp "$LAB_DIR/Makefile" "$WORK/"
else
    fail "missing Makefile in $LAB_DIR"
fi
[ $FAIL -gt 0 ] && { echo "Cannot continue."; exit 1; }
cd "$WORK" || exit 1

# ---------------------------------------------------------------- build
echo "== Build"
rm -f lab9 *.o
if make lab9 > build.log 2>&1 && [ -x lab9 ]; then
    pass "make builds lab9"
else
    fail "make lab9 failed"; sed 's/^/         /' build.log; exit 1
fi
if grep -qi "warning" build.log; then
    info "compiler warnings:"; grep -i "warning" build.log | sed 's/^/         /'
fi

# ---------------------------------------------------------------- source checks
echo "== Source checks (lab9.c)"
# Strip comments so commented-out code does not count.
gcc -fpreprocessed -dD -E -P lab9.c 2>/dev/null > lab9.nocomment.c || cp lab9.c lab9.nocomment.c
src_has() { grep -Eq "$1" lab9.nocomment.c; }
for fn in pthread_create pthread_join pthread_mutex_lock pthread_mutex_unlock sleep rand_r malloc free; do
    src_has "\\b$fn[[:space:]]*\\(" && pass "uses $fn()" || fail "$fn() not found (or commented out)"
done

MAX=$(sed -nE 's/^[[:space:]]*#[[:space:]]*define[[:space:]]+MAX_THREADS[[:space:]]+\(?([0-9]+)\)?.*/\1/p' lab9.nocomment.c | head -1)
if [ -n "$MAX" ]; then
    info "MAX_THREADS = $MAX"
else
    info "MAX_THREADS not found; expecting exactly n threads"
fi

# ---------------------------------------------------------------- helpers
now_ms() { date +%s%3N; }

# Line-buffer stdout so the output printed before a deadlock is not lost when we kill it.
LINEBUF=()
command -v stdbuf > /dev/null && LINEBUF=(stdbuf -oL)

# run_lab <n> <outfile> : runs ./lab9 n, sets RC, TIMED_OUT, ELAPSED_MS, MAX_THREADS_SEEN
run_lab() {
    local n=$1 out=$2 limit_ms=$(( ($1 + 8) * 1000 )) start t pid
    start=$(now_ms)
    "${LINEBUF[@]}" ./lab9 "$n" > "$out" 2>&1 &
    pid=$!
    MAX_THREADS_SEEN=0
    TIMED_OUT=0
    while kill -0 "$pid" 2>/dev/null; do
        t=$(awk '$1=="Threads:" {print $2}' "/proc/$pid/status" 2>/dev/null)
        [ -n "$t" ] && [ "$t" -gt "$MAX_THREADS_SEEN" ] && MAX_THREADS_SEEN=$t
        if [ $(( $(now_ms) - start )) -gt $limit_ms ]; then
            kill -9 "$pid" 2>/dev/null; TIMED_OUT=1; break
        fi
        # stop sampling once the process is a zombie
        grep -q '^State:[[:space:]]*Z' "/proc/$pid/status" 2>/dev/null && break
        sleep 0.05
    done
    wait "$pid" 2>/dev/null
    RC=$?
    ELAPSED_MS=$(( $(now_ms) - start ))
}

# check_output <outfile> <expected_passengers>
# Prints one "ERR <msg>" line per problem, then "TICKETS <id> <id> ..." with the
# passengers in the order they got their tickets.
check_output() {
    awk -v want="$2" '
    # number right after the first occurrence of key in s ("" if none)
    function after(s, key,   i, r) {
        i = index(s, key); if (!i) return ""
        r = substr(s, i + length(key)); sub(/[^0-9].*$/, "", r); return r
    }
    function err(msg) { if (++ne <= 8) print "ERR " msg }
    function rec(id, ev) {
        if (id == "") { err(sprintf("line %d: cannot read the passenger number: %s", NR, $0)); return }
        if ((id, ev) in L) { dup[id] = 1; return }
        L[id, ev] = NR; T[id, ev] = t; seen[id] = 1
    }
    BEGIN {
        nev = split("enter explore finish first second queue", EV, " ")
        TXT["enter"] = "entered the park";          TXT["explore"] = "is exploring the park"
        TXT["finish"] = "finished exploring";       TXT["first"] = "First Print"
        TXT["second"] = "Second Print";             TXT["queue"] = "has entered the ride queue"
    }
    {
        t = after($0, "[Time: ")
        if ((t == "" || index($0, "[Time: ") != 1) && $0 ~ /Passenger|Print/) {
            err(sprintf("line %d: does not start with \"[Time: t]\": %s", NR, $0))
            bad_time = 1
        }
    }
    /^- Number of passenger threads:/ { hdr = after($0, "threads: "); next }
    /entered the park/                { rec(after($0, "Passenger "), "enter"); next }
    /is exploring the park/           { rec(after($0, "Passenger "), "explore"); next }
    /finished exploring/              { rec(after($0, "Passenger "), "finish"); next }
    /First Print/ {
        id = after($0, "Passenger "); rec(id, "first")
        if (open != "")
            err(sprintf("line %d: \"First Print\" of passenger %s while passenger %s has not printed its \"Second Print\" yet (ticket booth not protected by ticket_mutex)", NR, id, open))
        open = id; next
    }
    /Second Print/ {
        id = after($0, "Passenger "); rec(id, "second")
        if (open == "")     err(sprintf("line %d: \"Second Print\" of passenger %s without its \"First Print\" right before it", NR, id))
        else if (id != open) err(sprintf("line %d: \"Second Print\" of passenger %s follows the \"First Print\" of passenger %s (prints of different passengers interleaved)", NR, id, open))
        open = ""
        order = order " " id
        k = after($0, "ticket #")
        if (k == "")         err(sprintf("line %d: no \"ticket #k\" in: %s", NR, $0))
        else if (k != ++nt)  err(sprintf("line %d: ticket #%s, expected #%d (tickets_sold must count 1, 2, 3, ... under ticket_mutex)", NR, k, nt))
        next
    }
    /has entered the ride queue/ {
        id = after($0, "Passenger "); rec(id, "queue")
        s = after($0, "queue size: ")
        if (s == "")         err(sprintf("line %d: no \"queue size: s\" in: %s", NR, $0))
        else if (s != ++nq)  err(sprintf("line %d: queue size %s, expected %d (ride_queue_size must count 1, 2, 3, ... under queue_mutex)", NR, s, nq))
        next
    }
    /All [0-9]+ passengers/ {
        summary = 1
        sa = after($0, "All "); ss = after($0, "tickets sold: "); sq = after($0, "ride queue size: ")
        next
    }
    END {
        if (hdr == "")        err("missing \"- Number of passenger threads: n\" line")
        else if (hdr != want) err(sprintf("header says %s passenger threads, expected %d", hdr, want))

        for (id in seen) if (id + 0 >= want) err(sprintf("passenger %s is out of range 0..%d", id, want - 1))
        for (id in dup)  err(sprintf("passenger %s printed the same step more than once (each thread needs a pointer to its own passenger_t, not &i of the loop)", id))

        for (i = 0; i < want; i++) {
            if (!(i in seen)) { missing = missing " " i; continue }
            for (j = 1; j <= nev; j++) {
                if (!((i, EV[j]) in L)) { err(sprintf("passenger %d never printed \"%s\"", i, TXT[EV[j]])); continue }
                if (j > 1 && ((i, EV[j-1]) in L) && L[i, EV[j]] < L[i, EV[j-1]])
                    err(sprintf("passenger %d: \"%s\" (line %d) comes before \"%s\" (line %d)", i, TXT[EV[j]], L[i, EV[j]], TXT[EV[j-1]], L[i, EV[j-1]]))
            }
            if (bad_time) continue
            if (((i, "explore") in T) && ((i, "finish") in T)) {
                d = T[i, "finish"] - T[i, "explore"]
                if (d < 1 || d > 4) err(sprintf("passenger %d explored for %d s (Time %d -> %d), expected a random 1-3 s", i, d, T[i, "explore"], T[i, "finish"]))
            }
            if (((i, "first") in T) && ((i, "second") in T)) {
                d = T[i, "second"] - T[i, "first"]
                if (d < 1 || d > 2) err(sprintf("passenger %d: First Print at Time %d, Second Print at Time %d, expected sleep(1) between them", i, T[i, "first"], T[i, "second"]))
            }
        }
        if (missing != "") err("these passengers never printed anything:" missing)
        if (open != "")    err(sprintf("passenger %s never printed its \"Second Print\"", open))

        if (!summary) err("missing final \"[Time: t] All n passengers have tickets (tickets sold: k, ride queue size: s)\" line")
        else if (sa != want || ss != want || sq != want)
            err(sprintf("final line reports %s passengers, %s tickets sold, ride queue size %s; expected %d, %d, %d", sa, ss, sq, want, want, want))

        if (ne > 8) print "ERR ... (" ne - 8 " more)"
        print "TICKETS" order
    }' "$1"
}

# ---------------------------------------------------------------- no argument
echo "== ./lab9 (no argument)"
timeout 5 ./lab9 > noarg.out 2>&1
rc=$?
if [ $rc -eq 124 ]; then
    fail "./lab9 without an argument hangs"
elif [ $rc -ne 0 ] && [ -s noarg.out ]; then
    pass "prints a usage message and exits with status $rc"
else
    fail "without an argument it should print a message and exit with a nonzero status (status $rc)"
fi

# ---------------------------------------------------------------- runs
test_run() {
    local n=$1 want=$1 out="run_$1.out" res order lo hi
    [ -n "$MAX" ] && [ "$MAX" -lt "$n" ] && want=$MAX
    echo "== ./lab9 $n  (expecting $want passenger(s), ~$((want + 1))-$((want + 3)) s)"
    run_lab "$n" "$out"

    if [ $TIMED_OUT -eq 1 ]; then
        fail "timed out after $((ELAPSED_MS / 1000)) s: deadlock? (a lock that is never released, or two locks taken in opposite orders)"
        grep -q . "$out" && info "last line before the hang: $(tail -n 1 "$out")"
    elif [ $RC -eq 0 ]; then
        pass "exits with status 0"
    elif [ $RC -gt 128 ]; then
        fail "crashed with signal $((RC - 128)) (e.g. 11 = segmentation fault: a thread read past the end of passengers?)"
    else
        fail "exit status $RC"
    fi

    res=$(check_output "$out" "$want")
    order=$(echo "$res" | sed -n 's/^TICKETS *//p')
    if echo "$res" | grep -q '^ERR'; then
        fail "output is wrong:"
        echo "$res" | sed -n 's/^ERR /         - /p'
    else
        pass "all $want passengers: park -> explore -> ticket -> ride queue, booth prints never interleaved, tickets #1..#$want, queue sizes 1..$want"
        [ -n "$order" ] && info "ticket order (passenger ids): $order"
    fi

    if [ ! -s "$out" ] && [ $ELAPSED_MS -lt 500 ]; then
        info "no output at all: main probably returns before the threads run (missing pthread_join?)"
    fi

    # timing: exploring takes >= 1 s, then the 1 s booth sleeps run one at a time
    lo=$(( (want + 1) * 1000 - 300 )); hi=$(( (want + 3) * 1000 + 2500 ))
    if [ $TIMED_OUT -eq 1 ]; then
        :
    elif [ $ELAPSED_MS -ge $lo ] && [ $ELAPSED_MS -le $hi ]; then
        pass "took $(printf '%d.%02d' $((ELAPSED_MS / 1000)) $((ELAPSED_MS % 1000 / 10))) s (expected $((want + 1))-$((want + 3)) s)"
    elif [ $ELAPSED_MS -lt $lo ]; then
        fail "took only $ELAPSED_MS ms, expected at least $((want + 1)) s: sleep(1) must be between lock and unlock of ticket_mutex, and main must pthread_join all threads"
    else
        fail "took $ELAPSED_MS ms, expected at most ~$((want + 3)) s (exploring is 1-3 s, the booth is 1 s per passenger)"
    fi

    # concurrency: all threads should exist at once (main + want threads)
    if [ "$want" -gt 1 ]; then
        if [ "$MAX_THREADS_SEEN" -eq 0 ]; then
            info "process exited too fast to count its threads"
        elif [ "$MAX_THREADS_SEEN" -ge $((want + 1)) ]; then
            pass "$want passenger threads ran at the same time (saw $MAX_THREADS_SEEN threads incl. main)"
        else
            fail "at most $MAX_THREADS_SEEN threads (incl. main) existed at once, expected $((want + 1)): create all threads first, then join them in a second loop"
        fi
    fi

    echo "  ---- program output ----"
    sed 's/^/  | /' "$out" | head -70
}

for n in "${COUNTS[@]}"; do
    test_run "$n"
done

# ---------------------------------------------------------------- MAX_THREADS cap
if [ -n "$MAX" ] && [ "$MAX" -le 10 ]; then
    echo "== ./lab9 $((MAX + 2))  (more than MAX_THREADS = $MAX)"
    run_lab $((MAX + 2)) cap.out
    got=$(grep -c 'First Print' cap.out)
    if [ $TIMED_OUT -eq 0 ] && [ $RC -eq 0 ] && [ "$got" -eq "$MAX" ]; then
        pass "n is capped at MAX_THREADS ($got passengers)"
    else
        fail "expected $MAX passengers for n = $((MAX + 2)), got $got (status $RC$([ $TIMED_OUT -eq 1 ] && echo ", timed out"))"
    fi
fi

# ---------------------------------------------------------------- valgrind
echo "== valgrind ./lab9 2"
if [ -n "$NOVALGRIND" ]; then
    info "skipped (NOVALGRIND is set)"
elif ! command -v valgrind > /dev/null; then
    info "valgrind is not installed, skipped"
else
    timeout 60 valgrind --leak-check=full --errors-for-leak-kinds=definite,indirect \
        --error-exitcode=99 ./lab9 2 > valgrind.out 2> valgrind.log
    rc=$?
    if [ $rc -eq 0 ]; then
        pass "no memory errors or leaks"
    elif [ $rc -eq 124 ]; then
        fail "valgrind run timed out (deadlock?)"
    else
        fail "valgrind reports problems (status $rc):"
        grep -E "ERROR SUMMARY|definitely lost|indirectly lost|Invalid|uninitialised" valgrind.log | head -8 | sed 's/^/         /'
    fi
fi

# ---------------------------------------------------------------- summary
echo
echo "== Summary: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
