#!/bin/bash

BOOGIE=~/Workspace/boogie/Binaries/Boogie.exe
CORRAL=~/Workspace/corral/bin/Debug/corral.exe

total=0
passed=0
failed=0

mkdir output
for sol in *.sol; do
    total=$((total+1))
    echo "======= $sol ======="
    echo "- Compiling"
    solc --boogie "$sol" -o ./output --overwrite > /dev/null
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
        mono $BOOGIE "$bpl" /doModSetAnalysis /nologo /errorTrace:0 > "$out"
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