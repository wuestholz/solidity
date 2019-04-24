#!/usr/bin/env bash

#
# Shell script for installing solc-verify testing dependencies
#

sudo apt-get install -y \
	mono-complete \
	python3 \
	python3-pip

sudo pip3 install psutil

# CVC4 1.7
wget https://github.com/CVC4/CVC4/releases/download/1.7/cvc4-1.7-x86_64-linux-opt -O cvc4
sudo install cvc4 /usr/bin/cvc4

# Yices2 2.6.1
pushd .
wget http://yices.csl.sri.com/releases/2.6.1/yices-2.6.1-x86_64-pc-linux-gnu-static-gmp.tar.gz
tar xzvf yices*
rm yices*.tar.gz
cd yices*
sudo ./install-yices
popd

# Boogie
pushd .
git clone https://github.com/dddejan/boogie.git
cd boogie
wget https://nuget.org/nuget.exe
travis_retry mono ./nuget.exe restore Source/Boogie.sln
xbuild Source/Boogie.sln
popd

# Truffle (use nvm, latest node)
source ~/.nvm/nvm.sh
nvm install node 
npm install -g truffle