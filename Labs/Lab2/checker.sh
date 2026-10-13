#!/bin/bash
#
# checker.sh - Self-check script for CS415 Lab2
#
# Runs the compiled ../lab2 from inside files/ and checks both modes:
#   1. Interactive mode: feeds "ls", "cat", "exit" on stdin and compares stdout.
#   2. File mode: runs "../lab2 -f ../input.txt" and compares files/output.txt.
#   3. Runs both modes under valgrind; any memory error or unfreed heap block fails.
#
# The expected "ls" line is built from the files/ directory itself: every
# .txt/.py file whose name starts with a digit (excluding output files),
# sorted by that leading number, each followed by a space.
#
# Usage:
#   ./checker.sh
#
# Exit status: 0 if everything passes, 1 otherwise.

set -u

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
cd "$SCRIPT_DIR" || exit 1

EXE="lab2"
FILES_DIR="files"
INPUT_FILE="input.txt"
PRODUCED_OUTPUT="$FILES_DIR/output.txt"

PASS=1

fail() {
	echo "FAIL: $1"
	PASS=0
}

# Compare expected vs actual text; print a diff on mismatch.
check_output() {
	local name="$1" expected="$2" actual="$3"
	if [ "$expected" == "$actual" ]; then
		echo "    PASS: $name"
	else
		fail "$name output does not match"
		diff -u --label expected --label actual \
			<(printf '%s' "$expected") <(printf '%s' "$actual") | cat -A
	fi
}

echo "==> Checking required files"
for f in "$INPUT_FILE"; do
	[ -f "$f" ] || fail "missing required file: $f"
done
[ -d "$FILES_DIR" ] || fail "missing required directory: $FILES_DIR"
[ "$PASS" -eq 1 ] || exit 1

echo "==> Building $EXE with make"
if ! make > build.log 2>&1; then
	fail "compilation failed:"
	cat build.log
	exit 1
fi
rm -f build.log
if [ ! -x "$EXE" ]; then
	fail "no executable named $EXE"
	exit 1
fi

# Expected "ls" output, e.g. "1_poem.txt 2_lyrics.txt 3_DE_Code.py "
LS_LINE="$(cd "$FILES_DIR" && ls -1 \
	| grep -E '^[0-9].*\.(txt|py)$' \
	| grep -vx -e 'output.txt' -e 'output_reference.txt' \
	| sort -n \
	| tr '\n' ' ')"
echo "    expected ls line: \"$LS_LINE\""

# ---------------------------------------------------------------------------
echo "==> [1] Interactive mode: (cd $FILES_DIR && ../$EXE) with ls, cat, exit"

EXPECTED_INTERACTIVE=">>> ${LS_LINE}
>>> Error! Unrecognized command: cat
>>> "

INTERACTIVE_LOG="$(mktemp)"
trap 'rm -f "$INTERACTIVE_LOG"' EXIT
(cd "$FILES_DIR" && printf 'ls\ncat\nexit\n' | timeout 5 "../$EXE") > "$INTERACTIVE_LOG"
STATUS=$?
ACTUAL_INTERACTIVE="$(cat "$INTERACTIVE_LOG"; echo x)"
ACTUAL_INTERACTIVE="${ACTUAL_INTERACTIVE%x}"   # keep trailing newlines/spaces intact

if [ "$STATUS" -eq 124 ]; then
	fail "interactive mode timed out (did \"exit\" end the program?)"
fi
check_output "interactive mode" "$EXPECTED_INTERACTIVE" "$ACTUAL_INTERACTIVE"

# ---------------------------------------------------------------------------
echo "==> [2] File mode: (cd $FILES_DIR && ../$EXE -f ../$INPUT_FILE)"

rm -f "$PRODUCED_OUTPUT"

# Build expected output.txt from input.txt: stop at "exit", skip blank lines.
EXPECTED_FILE=""
while IFS= read -r line || [ -n "$line" ]; do
	cmd="${line%% *}"
	[ -z "$cmd" ] && continue
	case "$cmd" in
		ls)   EXPECTED_FILE+="${LS_LINE}"$'\n' ;;
		exit) break ;;
		*)    EXPECTED_FILE+="Error! Unrecognized command: ${cmd}"$'\n' ;;
	esac
done < "$INPUT_FILE"

STDOUT_FILE_MODE="$(cd "$FILES_DIR" && timeout 5 "../$EXE" -f "../$INPUT_FILE" 2>&1)"
STATUS=$?
if [ "$STATUS" -eq 124 ]; then
	fail "file mode timed out"
elif [ "$STATUS" -ne 0 ]; then
	fail "file mode exited with status $STATUS"
fi
if [ -n "$STDOUT_FILE_MODE" ]; then
	fail "file mode should print nothing to the terminal, got: $STDOUT_FILE_MODE"
fi

if [ ! -f "$PRODUCED_OUTPUT" ]; then
	fail "file mode did not create $PRODUCED_OUTPUT"
else
	ACTUAL_FILE="$(cat "$PRODUCED_OUTPUT"; echo x)"
	ACTUAL_FILE="${ACTUAL_FILE%x}"
	check_output "file mode ($PRODUCED_OUTPUT)" "$EXPECTED_FILE" "$ACTUAL_FILE"
fi

# ---------------------------------------------------------------------------
# Runs valgrind on one mode. Any memory error or any leftover heap block
# (definitely/indirectly/possibly lost or still reachable) counts as a failure.
# Args: <label> <log file> <stdin text> <lab2 args...>
run_valgrind() {
	local label="$1" log="$2" input="$3"
	shift 3
	(cd "$FILES_DIR" && printf '%s' "$input" | timeout 60 valgrind \
		--leak-check=full --show-leak-kinds=all \
		--errors-for-leak-kinds=all --error-exitcode=99 \
		"../$EXE" "$@" > /dev/null 2> "../$log")
	local status=$?

	if [ "$status" -eq 124 ]; then
		fail "valgrind ($label) timed out. See $log"
	elif [ "$status" -eq 99 ] || ! grep -q "All heap blocks were freed" "$log"; then
		fail "valgrind ($label) found memory issues. See $log:"
		grep -E "Invalid|uninitialised|Mismatched|lost|reachable|ERROR SUMMARY" "$log"
	elif [ "$status" -ne 0 ]; then
		fail "valgrind ($label) exited with status $status. See $log"
	else
		echo "    PASS: $label (no errors, all heap blocks freed)"
		rm -f "$log"
	fi
}

echo "==> [3] Valgrind"
if ! command -v valgrind >/dev/null 2>&1; then
	fail "valgrind is not installed"
else
	run_valgrind "interactive mode" valgrind_interactive.log $'ls\ncat\nexit\n'
	run_valgrind "file mode" valgrind_file.log "" -f "../$INPUT_FILE"
fi

# ---------------------------------------------------------------------------
echo
if [ "$PASS" -eq 1 ]; then
	echo "RESULT: PASS"
	exit 0
else
	echo "RESULT: FAIL"
	exit 1
fi
