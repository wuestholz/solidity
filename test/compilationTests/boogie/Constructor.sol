pragma solidity ^0.4.23;

contract Constr {
    uint value;

    constructor(uint init) public {
        value = init;
    }

    function doSomething(uint param) public returns (uint) {
        value += param;
        return value;
    }
}