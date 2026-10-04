#!/bin/bash
#
# checker.sh - Self-check script for CS415 Lab1 (String processing in C)
#
# Builds lab1.exe with make, runs it on lab1_input.txt, compares the output
# against output_reference.txt byte for byte, then runs it under valgrind.
# Any memory error or unfreed heap block counts as a failure.
#
# Usage:
#   ./checker.sh
#
# Exit status: 0 if everything passes, 1 otherwise.

set -u

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
cd "$SCRIPT_DIR" || exit 1

EXE="lab1.exe"
INPUT_FILE="lab1_input.txt"
REFERENCE_FILE="output_reference.txt"
OUTPUT_FILE="output.txt"
BUILD_LOG="build.log"
VALGRIND_LOG="valgrind.log"

REQUIRED_FILES=(lab1_skeleton.c string_parser.c string_parser.h Makefile
	"$INPUT_FILE" "$REFERENCE_FILE")

OUTPUT_OK=1
LEAK_OK=1

# Print the failure summary and exit.
die() {
	echo "    $1"
	echo
	echo "RESULT: FAIL - $1"
	exit 1
}

echo "==> Checking required files"
for f in "${REQUIRED_FILES[@]}"; do
	[ -f "$f" ] || die "missing required file: $f"
done

echo "==> Cleaning previous build artifacts"
make clean > /dev/null 2>&1

echo "==> Compiling with make"
if ! make > "$BUILD_LOG" 2>&1; then
	cat "$BUILD_LOG"
	die "compilation failed (see $BUILD_LOG)"
fi
[ -x "$EXE" ] || die "make did not produce $EXE"
rm -f "$BUILD_LOG"
echo "    build OK"

echo "==> Running $EXE $INPUT_FILE"
timeout 10 "./$EXE" "$INPUT_FILE" > "$OUTPUT_FILE" 2>&1
STATUS=$?
if [ "$STATUS" -eq 124 ]; then
	die "$EXE timed out"
elif [ "$STATUS" -ne 0 ]; then
	die "$EXE exited with status $STATUS"
fi

echo "==> Comparing output against $REFERENCE_FILE"
if cmp -s "$OUTPUT_FILE" "$REFERENCE_FILE"; then
	echo "    output matches exactly"
else
	OUTPUT_OK=0
	echo "    output differs from reference (- expected, + actual):"
	diff -u --label "$REFERENCE_FILE" --label "$OUTPUT_FILE" \
		"$REFERENCE_FILE" "$OUTPUT_FILE" | cat -A | sed 's/^/    /'
fi

echo "==> Checking for memory leaks with valgrind"
if ! command -v valgrind > /dev/null 2>&1; then
	LEAK_OK=0
	echo "    valgrind is not installed"
else
	timeout 60 valgrind --leak-check=full --show-leak-kinds=all \
		--errors-for-leak-kinds=all --error-exitcode=99 \
		"./$EXE" "$INPUT_FILE" > /dev/null 2> "$VALGRIND_LOG"
	STATUS=$?
	if [ "$STATUS" -eq 124 ]; then
		LEAK_OK=0
		echo "    valgrind timed out (see $VALGRIND_LOG)"
	elif [ "$STATUS" -ne 0 ] || ! grep -q "All heap blocks were freed" "$VALGRIND_LOG"; then
		LEAK_OK=0
		echo "    memory errors or leaks detected (see $VALGRIND_LOG):"
		grep -E "Invalid|uninitialised|Mismatched|lost|reachable|ERROR SUMMARY" \
			"$VALGRIND_LOG" | sed 's/^/    /'
	else
		echo "    no leaks detected"
		rm -f "$VALGRIND_LOG"
	fi
fi

echo "==> Cleaning build artifacts"
make clean > /dev/null 2>&1

echo
if [ "$OUTPUT_OK" -eq 1 ] && [ "$LEAK_OK" -eq 1 ]; then
	echo "RESULT: PASS - output matches reference and no leaks detected"
	exit 0
fi

REASONS=()
[ "$OUTPUT_OK" -eq 1 ] || REASONS+=("output does not match reference")
[ "$LEAK_OK" -eq 1 ] || REASONS+=("memory errors or leaks detected")
echo "RESULT: FAIL - $(IFS=';'; echo "${REASONS[*]}" | sed 's/;/ and /g')"
exit 1
