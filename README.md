# The Solidity Contract-Oriented Programming Language
[![Join the chat at https://gitter.im/ethereum/solidity](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/ethereum/solidity?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge) [![Build Status](https://travis-ci.org/ethereum/solidity.svg?branch=develop)](https://travis-ci.org/ethereum/solidity)

## Useful links
To get started you can find an introduction to the language in the [Solidity documentation](https://solidity.readthedocs.org). In the documentation, you can find [code examples](https://solidity.readthedocs.io/en/latest/solidity-by-example.html) as well as [a reference](https://solidity.readthedocs.io/en/latest/solidity-in-depth.html) of the syntax and details on how to write smart contracts.

You can start using [Solidity in your browser](http://remix.ethereum.org) with no need to download or compile anything.

The changelog for this project can be found [here](https://github.com/ethereum/solidity/blob/develop/Changelog.md).

Solidity is still under development. So please do not hesitate and open an [issue in GitHub](https://github.com/ethereum/solidity/issues) if you encounter anything strange.

## Building
See the [Solidity documentation](https://solidity.readthedocs.io/en/latest/installing-solidity.html#building-from-source) for build instructions.

## How to Contribute
Please see our [contribution guidelines](https://solidity.readthedocs.io/en/latest/contributing.html) in the Solidity documentation.

Any contributions are welcome!

# Formal Verification Extension

This is an extended version of the compiler (v0.4.25) that is able to perform automated formal verification on Solidity code using annotations and modular program verification. This extension is currently under development and not all features of Solidity are supported yet (e.g. structs).

The extension requires [Z3](https://github.com/Z3Prover/z3) and [Boogie](https://github.com/boogie-org/boogie). The compiler can be [built as usual](https://solidity.readthedocs.io/en/latest/installing-solidity.html#building-from-source), but the path to the Boogie binary has to be supplied to `cmake` in the `-DBOOGIE_BIN` argument, e.g., use `cmake -DBOOGIE_BIN="boogie/Binaries" ..` instead of `cmake ..`.

After a successful build, the Python script `solc-verify.py` under `build/solc/` is the entry point for verification. For example: `./build/solc/solc-verify.py ./test/compilationTests/boogie/demo/DemoSpec.sol`. This will compile the contract (and its annotations) to the input language of Boogie, run Boogie and map the results back to the original file. For more information use the `-h` flag of the script and for more examples check the `test/compilationTests/boogie/` directory.

Optionally, you can use [CVC4](http://cvc4.cs.stanford.edu) instead of Z3, or you can also use [Yices2](https://github.com/SRI-CSL/yices2), but that requires an [extended version](https://github.com/dddejan/boogie) of Boogie.
