pragma solidity ^0.4.23;

/**
 * @notice invariant x == y
 * @notice some random comment
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

    function add_incorrect(uint amount) public {
        x += amount;
        y -= amount;
    }
}