pragma solidity ^0.4.23;

contract LocalVars {
    uint contractVar;

    function doSomething(uint param) public returns (uint) {
        uint uninitializedVar;
        uninitializedVar = 7;
        uint localVar1 = param;
        localVar1 = localVar1 + 2;
        uint localVar2 = 5;
        localVar2 = localVar1 + param;
        contractVar = localVar1 + localVar2;
        return contractVar;
    }
}