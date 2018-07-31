#!/bin/bash

# directory of the script
BOOGIE_TEST_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $BOOGIE_TEST_DIR
BASE_DIR=$BOOGIE_TEST_DIR/../../..


BOOGIE=$BASE_DIR/../boogie-yices/Binaries/Boogie.exe
CORRAL=$BASE_DIR/../corral/bin/Debug/corral.exe
SOLC=$BASE_DIR/build/solc/solc

total=0
passed=0
failed=0

mkdir output
for sol in *.sol; do
    total=$((total+1))
    echo "======= $sol ======="
    echo "- Compiling"
    flags=""
    if [ -f "$sol.flags" ]; then
        flags=$(<"$sol.flags")
    fi
    $SOLC --boogie "$sol" -o ./output --overwrite $flags > /dev/null
    bpl="./output/$sol.bpl"
    out="$bpl.out"
    exp="$sol.expected"
    grep "main\(\)" "$bpl" > /dev/null
    if [[ "$?" == "0" ]] 
    then
        echo -e "- Running Corral (main found)"
        mono $CORRAL "$bpl" /main:main /recursionBound:20 > "$out"
    else
        echo -e "- Running Boogie (no main)"
        mono $BOOGIE "$bpl" /doModSetAnalysis /nologo /errorTrace:0 /useArrayTheory /proverOpt:SOLVER=Yices2 > "$out"
    fi

    grep -f "$exp" "$out" > /dev/null
    if [[ "$?" == "0" ]] 
    then
        echo -e "- Output OK"
        passed=$((passed+1))
    else
        echo -e "- Error in output"
        failed=$((failed+1))
    fi
done

rm -rf output

echo "======= Done ======="
echo "$total tests, $passed passed, $failed failed"