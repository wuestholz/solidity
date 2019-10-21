# Instruction for running KLEE on a Solidity contract

## Step 0: Build Docker image with our solc extension

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
```

## Step 1: Translate Solidity code to C

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
$ docker run --rm -it -v /${PWD}:/inout smartie:latest bash -c 'cd /inout; /code/solidity-to-cmodel/build/run/bin/solc test.sol --c-model --output-dir=out'
```

## Step 2: Run KLEE Docker image

```
$ docker pull klee/klee:2.0
$ docker run --rm -ti --user root -v /${PWD}/out:/inout --ulimit='stack=-1:-1' klee/klee:2.0
```

## Step 3: Run KLEE on generated C code (inside running docker container)

```
$ cd /inout
$ clang-6.0 -I /home/klee/klee_src/include -D MC_USE_STDINT -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone cmodel.c libverify/verify_klee.c
$ llvm-link-6.0 -f cmodel.bc verify_klee.bc -o program.bc
$ /home/klee/klee_build/bin/klee --simplify-sym-indices --write-cvcs --write-cov --output-module --max-memory=1000 --disable-inlining --optimize --use-forked-solver --use-cex-cache --libc=uclibc --posix-runtime --external-calls=all --only-output-states-covering-new --max-sym-array-size=4096 --max-instruction-time=30s --max-time=60min --watchdog --max-memory-inhibit=false --max-static-fork-pct=1 --max-static-solve-pct=1 --max-static-cpfork-pct=1 --switch-type=internal --search=random-path --search=nurs:covnew --use-batching-search --batch-instructions=10000 --silent-klee-assume --max-forks=512 program.bc
```
