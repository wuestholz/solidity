# solc-verify

This is an extended version of the compiler (v0.4.25) that is able to perform automated formal verification on Solidity code using annotations and modular program verification. This extension is currently under development and not all features of Solidity are supported yet (e.g. structs).

## Build and Install

Solc-verify is mainly developed and tested on Ubuntu and OS X. It requires [Z3 (SMT solver)](https://github.com/Z3Prover/z3) and [Boogie](https://github.com/dddejan/boogie) as a verification backend. Alternatively, [CVC4](http://cvc4.cs.stanford.edu) or [Yices2](https://github.com/SRI-CSL/yices2) can also be used instead of Z3.

On a standard Ubuntu system, solc-verify can be built and installed as follows.

**Common tools**
```
sudo apt install -y mono-complete
sudo apt install -y cmake
sudo apt install -y libboost-all-dev
sudo apt install -y python3
sudo apt install python3-pip
pip3 install psutil
sudo apt install -y libgmp3-dev
sudo apt install -y m4
sudo apt install -y automake
sudo apt install -y default-jre
sudo apt install -y gperf
```

Create a folder named `solc-verify-tools` for solc-verify and its dependencies:
```
mkdir solc-verify-tools
cd solc-verify-tools
```

**[Z3 SMT solver](https://github.com/Z3Prover/z3)**
```
git clone https://github.com/Z3Prover/z3.git
cd z3/
git reset z3-4.8.4 --hard
python scripts/mk_make.py
cd build
make
sudo make install
cd ../..
```

**[CVC4 SMT solver](http://cvc4.cs.stanford.edu)** (Optional)
```
git clone https://github.com/CVC4/CVC4.git
cd CVC4
git reset b396d78 --hard
./contrib/get-antlr-3.4
./contrib/get-abc
./contrib/get-glpk-cut-log
./contrib/get-cadical
./configure.sh --abc --gpl --glpk --cadical
cd build
make
sudo make install
cd ../..
```

**[Yices2 SMT solver](https://github.com/SRI-CSL/yices2)** (Optional)
```
git clone https://github.com/SRI-CSL/yices2.git
cd yices2
git reset Yices-2.6.1 --hard
autoconf
./configure
make
sudo make install
cd ..
```

**[Boogie verifier](https://github.com/dddejan/boogie)**

_Note, that this is an extended version of Boogie, that supports Yices2._
```
git clone https://github.com/dddejan/boogie.git
cd boogie
wget https://nuget.org/nuget.exe
mono ./nuget.exe restore Source/Boogie.sln
xbuild Source/Boogie.sln
ln -s /usr/bin/z3 Binaries/z3.exe
cd ..
```

**[solc-verify](https://github.com/SRI-CSL/solidity)**
```
git clone https://github.com/SRI-CSL/solidity.git
cd solidity
git checkout boogie
./scripts/install_deps.sh
mkdir build
cd build
cmake -DBOOGIE_BIN="../../boogie/Binaries" ..
make
sudo make install
cd ../..
```

## Running solc-verify

The entry point of our tool is the script `solc-verify.py`. The script has a single positional argument that describes the path to the input file to be verified. You can type `solc-verify.py --help` to print the optional arguments, but we also list them below.

- `-h`, `--help`: Show help message and exit.
- `--timeout TIMEOUT`: Timeout for running the Boogie verifier in seconds (default is 10).
- `--output OUTPUT`: Output directory where the intermediate (e.g., Boogie) files are created (default is `.`).
- `--smtlog SMTLOG`: Log the inputs given by Boogie to the SMT solver into a file (not given by default).
- `--errors-only`: Only display error messages and omit displaying names of correct functions (not given by default).
- `--solc SOLC`: Path to the Solidity compiler to use (which must include our Boogie translator extension) (by default it is the one that includes the Python script).
- `--boogie BOOGIE`: Path to the Boogie verifier binary to use (by default it is the one given during building the tool).
- `--boogie-arith {int,bv,mod,mod-overflow}`: Arithmetic encoding mode to be used: SMT integers (`int`), bitvectors (`bv`), modulo arithmetic (`mod`), modular arithmetic with overflow detection (`mod-overflow`).
- `--solver {z3,yices2,cvc4}`: SMT solver used by the verifier (default is `z3`).
- `--solver-bin`: Path to the solver to be used, if not given, the solver is searched on the path (not given by default).

For example, the following command runs the tool on the `SimpleBank.sol` contract with CVC4 and 60 seconds timeout for the verifier (the contract is actually located under `test/compilationTests/boogie/examples/SimpleBank.sol` in this repository).
```
solc-verify.py SimpleBank.sol --solver cvc4 --timeout 60
```

## Examples

Some examples are located under the `test/compilationTests/boogie/examples` folder of the repository.

### Specifictaion Annotations

This example (`Annotations.sol`) presents the available specification annotations. A contract-level invariant (line 3) ensures that `x` and `y` are always equal. Non-public functions (such as `add_to_x` in line 10) are not checked against the contract-level invariant, but can be annotated with pre- and postconditions explicitly. Furthermore, loops can be annotated with loop invariants (such as in line 18). This contract is correct and can be verified by the following command:
```
solc-verify.py Annotations.sol
```
Note, that it is also free of overflows, since the programmer included an explicit check in line 12. Our tool can detect this and avoid a false alarm:
```
solc-verify.py Annotations.sol --boogie-arith mod-overflow
```
However, removing that check and running the verifier with overflow checks will report the error.

### SimpleBank

This is the simplified version of the DAO hack, illustrating the reentrancy issue. There are two versions of the `withdraw` function (line 13). In the incorrect version (`SimpleBankReentrancy.sol`) we first transfer the money and then reduce the balance of the sender, allowing a reentrancy attack. The operations in the correct version (`SimpleBankCorrect.sol`) are the other way around, preventing the reentrancy attack. The contract is annotated with a contract level invariant (line 4) ensuring that the sum of the individual balances equals to the balance of the contract itself. Using this invariant we can detect the error in the incorrect version (invariant does not hold when the reentrant call is made) and avoid a false alarm in the correct version (invariant holds when the reentrant call is made). The tool can be executed with the following commands:
```
solc-verify.py SimpleBankReentrancy.sol
solc-verify.py SimpleBankCorrect.sol
```

### BecToken

This example (`BecTokenSimplifiedOverflow.sol`) presents a part of the BecToken, which had an overflow issue. It uses the `SafeMath` library for most operations to prevent overflows, but there is a multiplication in `batchTransfer` (line 61) that can overflow. The function transfers a given `_value` to a given number of `_receivers`. It first reduces the balance of the sender with the product of the value and the number of receivers and then transfers the value to each receiver in a loop. If the product overflows, a small product will be reduced from the sender, but large values will be transferred to the receivers. Our tool can detect this issue by the following command (using CVC4):
```
solc-verify.py BecTokenSimplifiedOverflow.sol --boogie-arith mod-overflow --solver cvc4
```
In the correct version (`BecTokenSimplifiedCorrect.sol`), the multiplication in line 61 is replaced by the `mul` operation from `SafeMath`, making the contract safe. Our tool can not only prove the absence of overflows, but also the contract invariant (sum of balances equals to total supply, line 34) and the loop invariant (line 67) including nonlinear arithmetic:
```
solc-verify.py BecTokenSimplifiedCorrect.sol --boogie-arith mod-overflow --solver cvc4
```