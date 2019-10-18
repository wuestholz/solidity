# Instruction for running libfuzzer on a Solidity contract

## Step 0: Build our solc extension

You can use the following Dockerfile to build a docker image:

```
FROM ubuntu:18.04

WORKDIR /code
RUN apt-get update && \
    apt-get install -y --no-install-recommends sudo git ca-certificates lsb-release && \
    git clone https://github.com/ScottWe/solidity-to-cmodel.git && \
    cd solidity-to-cmodel && \
    git checkout cmodel-dev && \
    ./scripts/install_deps.sh && \
    mkdir build && \
    cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=run -DUSE_CVC4=OFF -DUSE_Z3=OFF && \
    make && \
    make install
```

```
$ docker build --rm -t "smartie:latest" .
$ docker run --rm -it smartie:latest bash
```

## Step 1: Install libfuzzer (included with clang 6.0 or later)

```
$ apt-get install -y --no-install-recommends clang-6.0
$ sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang-6.0 100
$ sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-6.0 100
```

## Step 2: Translate Solidity code to C

Create a Solidity contract in a file `test.sol`. For example:

```
pragma solidity ^0.5.0;
contract Contract {
	uint256 counter;
	function incr() public {
		counter = counter + 1;
		assert(counter < 2);
	}
}
```

```
$ mkdir out
$ solidity-to-cmodel/build/run/bin/solc test.sol --c-model --output-dir=out
```

## Step 3: Run libfuzzer

```
$ cd out
$ sed -i 's/int main/int not_main/g' cmodel.cpp
$ clang++-6.0 -g -fsanitize=address,fuzzer -D MC_USE_STDINT cmodel.cpp cmodel.h libverify/verify.h libverify/verify_libfuzzer.cpp
$ mkdir CORPUS_DIR
$ ./a.out CORPUS_DIR -max_len=16384 -runs=1000000 -timeout=5 -use_value_profile=1 -print_final_stats=1
```
