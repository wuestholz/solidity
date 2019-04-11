#!/usr/bin/env bash

#
# Shell script for installing solc-verify testing dependencies
#

sudo apt-get install -y \
	automake \
	default-jre \
	gperf \
	libgmp3-dev \
	m4 \
	mono-complete \
	python3 \
	python3-pip \
	realpath

sudo pip3 install psutil

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
