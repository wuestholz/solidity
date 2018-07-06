pragma solidity ^0.4.23;

contract LocalVars {
    uint contractVar;

    function init() public {
        contractVar = 10;
    }

    function doSomething(uint param) public returns (uint) {
        uint localVar = 5;
        localVar += param;
        contractVar += localVar;
        return contractVar;
    }

    function chained(uint param) public returns (uint) {
        uint localVar;
        localVar = contractVar = param + 1;
        return localVar;
    }

    function chained2(uint param) public returns (uint) {
        uint localVar = param;
        contractVar = localVar += 1;
        return contractVar;
    }

    function __verifier_main() public {
        init();
        assert(doSomething(20) == 35);
        assert(doSomething(10) == 50);
        assert(chained(5) == 6);
        assert(chained2(5) == 6);
    }
}