pragma solidity ^0.4.23;

/**
 * @invariant x == y
 */
contract SomeContract {
    uint x;
    uint y;

    constructor() public {
        x = y = 0;
    }

    function add(uint amount) public {
        x += amount;
        y += amount;
    }
}