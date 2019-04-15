pragma solidity >=0.5.0;

contract Constr {
    uint value;

    constructor(uint init) public {
        // Value should be initialize to 0 here
        assert(value == 0);
        value = init;
        assert(value == init);
    }

    function doSomething(uint param) public returns (uint) {
        value += param;
        return value;
    }
}