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

    function __verifier_main() public {
        init();
        assert(doSomething(20) == 35);
        assert(doSomething(10) == 50);
    }
}