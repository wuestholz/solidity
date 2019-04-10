#!/usr/bin/env bash

#
# Shell script for installing solc-verify testing dependencies
#

sudo apt-get install -y mono-complete
sudo apt-get install -y cmake
sudo apt-get install -y libboost-all-dev
sudo apt-get install -y python3
sudo apt-get install -y python3-pip
pip3 install psutil
sudo apt-get install -y libgmp3-dev
sudo apt-get install -y m4
sudo apt-get install -y automake
sudo apt-get install -y default-jre
sudo apt-get install -y gperf

# Yices2
pushd .
git clone https://github.com/SRI-CSL/yices2.git
cd yices2
autoconf
./configure
make
sudo make install
popd 

# Z3 
pushd .
git clone https://github.com/Z3Prover/z3.git
cd z3
python scripts/mk_make.py
cd build
make
sudo make install
popd

# CVC4
pushd .
git clone https://github.com/CVC4/CVC4.git
cd CVC4
./contrib/get-antlr-3.4
./configure.sh --gpl
cd build
make
sudo make install
popd

# Boogie
pushd .
git clone https://github.com/dddejan/boogie.git
cd boogie
wget https://nuget.org/nuget.exe
mono ./nuget.exe restore Source/Boogie.sln
xbuild Source/Boogie.sln
ln -s /usr/bin/z3 Binaries/z3.exe
popd
