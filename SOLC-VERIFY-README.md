# solc-verify

This is an extended version of the compiler that is able to perform automated formal verification on Solidity code using annotations and modular program verification. More information can be found in this readme.

A preliminary [paper](https://arxiv.org/abs/1907.04262) was presented at [VSTTE'19](https://sri-csl.github.io/VSTTE19/), but the tool had major improvements since then, including support for Solidity 0.5.x, complex datatypes, memory models and new kinds of specification annotations. Up-to-date information can be found in this readme.

## Build and Install

Solc-verify is mainly developed and tested on Ubuntu and OS X. It requires [Z3](https://github.com/Z3Prover/z3) or [CVC4](http://cvc4.cs.stanford.edu), and [Boogie](https://github.com/boogie-org/boogie) as a verification backend. On a standard Ubuntu system (18.04), solc-verify can be built and installed as follows.

**[CVC4](http://cvc4.cs.stanford.edu)** (>=1.6 required)
```
wget http://cvc4.cs.stanford.edu/downloads/builds/x86_64-linux-opt/cvc4-1.7-x86_64-linux-opt
chmod a+x cvc4-1.7-x86_64-linux-opt
sudo cp cvc4-1.7-x86_64-linux-opt /usr/local/bin/cvc4
```

**[Mono](https://www.mono-project.com/download/stable/#download-lin)** (required for Boogie)
```
sudo apt install gnupg ca-certificates -y
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF
echo "deb https://download.mono-project.com/repo/ubuntu stable-bionic main" | sudo tee /etc/apt/sources.list.d/mono-official-stable.list
sudo apt update
sudo apt install mono-devel -y
```

**[Boogie](https://github.com/boogie-org/boogie)**
```
git clone https://github.com/boogie-org/boogie.git
cd boogie
wget https://nuget.org/nuget.exe
mono ./nuget.exe restore Source/Boogie.sln
xbuild Source/Boogie.sln
cd ..
```

**solc-verify**
```
sudo apt install python3-pip -y
pip3 install psutil
git clone https://github.com/SRI-CSL/solidity.git
cd solidity
./scripts/install_deps.sh
mkdir build
cd build
cmake -DBOOGIE_BIN="<PATH TO BOOGIE BINARIES FOLDER>" ..
make
sudo make install
cd ../..
```

## Running solc-verify

The entry point is the script `solc-verify.py`. The script has a single positional argument that describes the path to the input Solidity file to be verified. You can type `solc-verify.py --help` to print the optional arguments, but we also list them below.

- `-h`, `--help`: Show help message and exit.
- `--timeout TIMEOUT`: Timeout for running the Boogie verifier in seconds (default is 10).
- `--arithmetic {int,bv,mod,mod-overflow}`: Arithmetic encoding mode to be used: SMT integers (`int`), bitvectors (`bv`), modulo arithmetic (`mod`), modular arithmetic with overflow detection (`mod-overflow`).
- `--modifies-analysis`: State variables are checked for modifications only if there are modification annotations or if this flag is given.
- `--output OUTPUT`: Output directory where the intermediate (e.g., Boogie) files are created (tmp directory by default).
- `--verbose`: Print all output of the compiler and the verifier.
- `--smt-log SMTLOG`: Log the inputs given by Boogie to the SMT solver into a file (not given by default).
- `--errors-only`: Only display error messages and omit displaying names of correct functions (not given by default).
- `--show-warnings`: Display warning messages (not given by default).
- `--solc SOLC`: Path to the Solidity compiler to use (which must include our Boogie translator extension) (by default it is the one that includes the Python script).
- `--boogie BOOGIE`: Path to the Boogie verifier binary to use (by default it is the one given during building the tool).
- `--solver {z3,cvc4}`: SMT solver used by the verifier (default is detected during compile time).
- `--solver-bin`: Path to the solver to be used, if not given, the solver is searched on the system path (not given by default).

For example, the following command runs the tool on the [`SimpleBankCorrect.sol`](test/solc-verify/examples/SimpleBankCorrect.sol) contract with CVC4 and 60 seconds timeout for the verifier.
```
solc-verify.py SimpleBankCorrect.sol --solver cvc4 --timeout 60
```

## Examples

Some examples are located under the `test/solc-verify/examples`.

### Specifictaion Annotations

This example ([`Annotations.sol`](test/solc-verify/examples/Annotations.sol)) presents the available specification annotations. A contract-level invariant (line 3) ensures that `x` and `y` are always equal. Contract-level annotations are added as both pre- and postconditions to public functions. Non-public functions (such as `add_to_x`) are not checked against the contract-level invariant, but can be annotated with pre- and post-conditions explicitly. By default, non-public functions are inlined to a depth of 1. Loops can be annotated with loop invariants. Furthermore, functions can be annotated with the state variables that they can modify (including conditions). This contract is correct and can be verified by the following command:
```
solc-verify.py Annotations.sol
```
Note, that it is also free of overflows, since the programmer included an explicit check in line 13. Our tool can detect this and avoid a false alarm:
```
solc-verify.py Annotations.sol --arithmetic mod-overflow
```
However, removing that check and running the verifier with overflow checks will report the error.

### SimpleBank

This is the simplified version of the DAO hack, illustrating the reentrancy issue. There are two versions of the `withdraw` function (line 13). In the incorrect version ([`SimpleBankReentrancy.sol`](test/solc-verify/examples/SimpleBankReentrancy.sol)) we first transfer the money and then reduce the balance of the sender, allowing a reentrancy attack. The operations in the correct version ([`SimpleBankCorrect.sol`](test/solc-verify/examples/SimpleBankCorrect.sol)) are the other way around, preventing the reentrancy attack. The contract is annotated with a contract level invariant (line 4) ensuring that the sum of the individual balances equals to the balance of the contract itself. Using this invariant we can detect the error in the incorrect version (invariant does not hold when the reentrant call is made) and avoid a false alarm in the correct version (invariant holds when the reentrant call is made). The tool can be executed with the following commands:
```
solc-verify.py SimpleBankReentrancy.sol
solc-verify.py SimpleBankCorrect.sol
```

### Sum over struct members

This example ([`SumOverStructMember.sol`](test/solc-verify/examples/SumOverStructMember.sol)) presents a modified version of the simple bank, where the accounts are complex structures and the sum is expressed over a member of this structure.
```
solc-verify.py SumOverStructMember.sol
```

### BecToken

This example ([`BecTokenSimplifiedOverflow.sol`](test/solc-verify/examples/BecTokenSimplifiedOverflow.sol)) presents a part of the BecToken, which had an overflow issue. It uses the `SafeMath` library for most operations to prevent overflows, but there is a multiplication in `batchTransfer` (line 61) that can overflow. The function transfers a given `_value` to a given number of `_receivers`. It first reduces the balance of the sender with the product of the value and the number of receivers and then transfers the value to each receiver in a loop. If the product overflows, a small product will be reduced from the sender, but large values will be transferred to the receivers. Our tool can detect this issue by the following command (using CVC4):
```
solc-verify.py BecTokenSimplifiedOverflow.sol --arithmetic mod-overflow --solver cvc4
```
In the correct version ([`BecTokenSimplifiedCorrect.sol`](test/solc-verify/examples/BecTokenSimplifiedCorrect.sol)), the multiplication in line 61 is replaced by the `mul` operation from `SafeMath`, making the contract safe. Our tool can not only prove the absence of overflows, but also the contract invariant (sum of balances equals to total supply, line 34) and the loop invariant (line 67) including nonlinear arithmetic:
```
solc-verify.py BecTokenSimplifiedCorrect.sol --arithmetic mod-overflow --solver cvc4
```

### Storage

This example ([`Storage.sol`](test/solc-verify/examples/Storage.sol)) presents a simple storage example, where each user can set, update or clear their data (represented as an integer) in the storage. The owner can clear any data. This example demonstrates annotation possibilities (such as modifications) over complex datatypes (such as structs and mappings).
```
solc-verify.py Storage.sol
```

## Specification Annotations

Specification annotations must be included in special documentation comments (`///` or `/** */`) and must start with the special doctag `@notice`.
They must be side-effect free Solidity expressions and can refer to variables within the scope of the annotated element. Functions cannot be called in the annotations, except for getters. The currently available annotations are listed below. We try to keep the language simple to enable automation, but it is evolving based on user input.

See the contracts under `test/solc-verify/examples` for examples.

- **Function pre/postconditions** (`precondition <EXPRESSION>` / `postcondition <EXPRESSION>`) can be attached to functions. Preconditions are assumed before executing the function and postconditions are asserted in the end. The expression can refer to variables in the scope of the function. The postcondition can also refer to the return value if it is named.
- **Contract level invariants**  (`invariant <EXPRESSION>`) can be attached to contracts. They are included as both a pre- and a postcondition for each _public_ function. The expression can refer to state variables in the contract.
- **Loop invariants**  (`invariant <EXPRESSION>`) can be attached to _for_ and _while_ loops. The expression can refer to variables in scope of the loop, including the loop counter.
- **Modification specifiers** (`modifies <TARGET> [if <CONDITION>]`) can be attached to functions. The target can be a state variable, including index and member accesses. Optionally, a condition can also be given. They will be checked at the end of the function (whether only the specified variables were modified). See [`Storage.sol`](test/solc-verify/examples/Storage.sol).
- Contract and loop invariants can refer to a special **sum function over collections** (`__verifier_sum_int` or `__verifier_sum_uint`). The argument must be an array/mapping state variable with integer values, or must point to an integer member if the array/mapping contains structures (see [`SumOverStructMember.sol`](test/solc-verify/examples/SumOverStructMember.sol)).
- Postconditions can refer to the **old value** of a variable (before the transaction) using `__verifier_old_<TYPE>` (e.g., `__verifier_old_uint(...)`).
- Specifications can refer to a special **equality predicate** (`__verifier_eq`) for reference types such as structures, arrays and mappings (not comparable with the standard Solidity operator `==`). It takes two arguments with the same type. For storage data location it performs a deep equality check, for other data locations it performs a reference equality check.
