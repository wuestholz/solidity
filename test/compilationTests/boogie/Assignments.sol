pragma solidity ^0.4.17;

contract LocalVars {
    uint contractVar;

    function doSomething(uint param) public returns (uint) {
        uint localVar = 5;
        localVar += param;
        contractVar += localVar;
        return contractVar;
    }
}