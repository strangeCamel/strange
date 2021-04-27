#!/bin/bash

TMP=/tmp/strange.$$.tmp
TRIE=/tmp/strange.$$.trie
OUT=/tmp/strange.$$.out

FAILED=0

RESULTS="../_results"


function Test_Failed
{
	cat "$OUT"
	echo $'\033[1;31m'"FAILED TEST $1 - $2"$'\033[m' >&2
	((FAILED++))

	mkdir -p "$RESULTS/test/$1.fail"

	cp "$TRIE" "$RESULTS/test/$1.fail/trie"
	cp "$TMP" "$RESULTS/test/$1.fail/tmp"
	cp "$OUT" "$RESULTS/test/$1.fail/out"
}

function Test_Eval
{
	while IFS= read -r line; do
		echo "$line" > "$TMP"
		if ! "$RESULTS/strange" -load "$TRIE" -eval "$TMP" >> "$OUT"; then
			Test_Failed "$1" "line mismatch unexpected: $line"
		fi
	done < "$1/eval-match"

	if ! "$RESULTS/strange" -load "$TRIE" -eval "$1/eval-match" >> "$OUT"; then
		Test_Failed "$1" "file mismatch unexpected: eval-match"
	fi

	while IFS= read -r line; do
		if [ "$line" != "" ]; then
			echo "$line" > "$TMP"
			if "$RESULTS/strange" -load "$TRIE" -eval "$TMP" >> "$OUT"; then
				Test_Failed "$1" "line match unexpected: $line"
			fi
		fi
	done < "$1/eval-mismatch"

	if "$RESULTS/strange" -load "$TRIE" -eval "$1/eval-mismatch" >> "$OUT"; then
		Test_Failed "$1" "file match unexpected: eval-match"
	fi
}

function Test_Run
{
	rm -f "$OUT" "$TRIE" "$TMP"
	"$RESULTS/strange" -learn ./$1/sample.* -save "$TRIE" >> "$OUT"
	Test_Eval "$1"
	rm -f "$TRIE" "$TMP"

	echo "" >> "$OUT"
	echo " --- " >> "$OUT"

	LOAD_ARG=()
	for f in `ls ./$1/sample.* | sort -V`; do
		"$RESULTS/strange" "${LOAD_ARG[@]}" -learn "$f" -save "$TRIE" >> "$OUT"
		LOAD_ARG=(-load "$TRIE")
	done
	Test_Eval "$1"
	rm -f "$OUT" "$TRIE" "$TMP"
}

rm -rf "$RESULTS/test/"

for t in `ls -d * | sort -V`; do
	if [[ -d "./$t" ]]; then
		echo $'\033[1m'" -- TESTING: $t"$'\033[m' >&2
		PREV_FAILED=$FAILED
		Test_Run "$t"
		if [ "$PREV_FAILED" == "$FAILED" ]; then
			echo $'\033[1m'" -> PASSED: $t"$'\033[m' >&2
		else
			echo $'\033[1;33m'" -> FAILED: $t"$'\033[m' >&2
		fi
	fi
done

if [ $FAILED -ne 0 ]; then
	echo $'\033[1;33m'"FAILS_COUNT: $FAILED"$'\033[m' >&2
fi

exit $FAILED

