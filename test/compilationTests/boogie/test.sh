#!/bin/bash

# directory of the script
BOOGIE_TEST_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $BOOGIE_TEST_DIR
BASE_DIR=$BOOGIE_TEST_DIR/../../..

SOLC_VERIFY=$BASE_DIR/build/solc/solc-verify.py

total=0
passed=0
failed=0

mkdir output
for sol in *.sol; do
    total=$((total+1))
    echo -n "- $sol: "
    flags=""
    if [ -f "$sol.flags" ]; then
        flags=$(<"$sol.flags")
    fi
    out="./output/$sol.out"
    $SOLC_VERIFY "$sol" --output "./output" $flags > "$out"
    exp="$sol.expected"

    cmp -s "$exp" "$out" > /dev/null
    if [[ "$?" == "0" ]] 
    then
        echo -e "OK"
        passed=$((passed+1))
    else
        echo -e "ERROR"
        failed=$((failed+1))
    fi
done

rm -rf output

echo "======= Done ======="
echo "$total tests, $passed passed, $failed failed"