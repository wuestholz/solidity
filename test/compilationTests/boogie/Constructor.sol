pragma solidity ^0.4.23;

contract Constr {
    uint value;

    constructor(uint init) public {
        // Value should be initialize to 0 here
        value = init;
    }

    function doSomething(uint param) public returns (uint) {
        value += param;
        return value;
    }
}