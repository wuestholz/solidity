#!/bin/bash

BOOGIE=~/Workspace/boogie/Binaries/Boogie.exe
CORRAL=~/Workspace/corral/bin/Debug/corral.exe

mkdir output
for f in *.sol; do
    solc --boogie "$f" -o ./output --overwrite
done

echo -e "\n======= Running verifier =======\n"

for f in ./output/*.bpl; do
    grep "main\(\)" "$f" > /dev/null
    if [[ "$?" == "0" ]] 
    then
        echo -e "\n======= $f (with main function, running Corral) =======\n"
        mono $CORRAL "$f" /main:main /recursionBound:20
    else
        echo -e "\n======= $f (without main function, running Boogie) =======\n"
        mono $BOOGIE "$f" /doModSetAnalysis /nologo /errorTrace:0
    fi
done

rm -rf output