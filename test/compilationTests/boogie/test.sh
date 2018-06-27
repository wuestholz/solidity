#!/bin/bash

BOOGIE=~/Workspace/boogie/Binaries/Boogie.exe

mkdir output
for f in *.sol; do
    solc --boogie "$f" -o ./output --overwrite
done

echo "======= Running Boogie ======="

for f in ./output/*.bpl; do
    echo "======= $f ======="
    mono $BOOGIE "$f" /doModSetAnalysis
done

rm -rf output