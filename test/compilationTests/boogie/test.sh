#!/bin/bash

#BOOGIE=~/Workspace/boogie/Binaries/Boogie.exe
CORRAL=~/Workspace/corral/bin/Debug/corral.exe

mkdir output
for f in *.sol; do
    solc --boogie "$f" -o ./output --overwrite
done

echo "======= Running Corral ======="

for f in ./output/*.bpl; do
    echo "======= $f ======="
    mono $CORRAL "$f" /main:main
done

rm -rf output