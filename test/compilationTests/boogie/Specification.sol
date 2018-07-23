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
        y -= amount; // ERROR
    }

    function add_incorrect_2(uint amount) public {
        x += amount;
        add(amount); // ERROR, invariant does not hold when calling function
        y += amount;
    }
}

/**
 * @notice invariant x == y
 */
contract DefaultConstructor {
    // The generated default constructor (setting the initial values) should fail
    uint x = 0;
    uint y = 1;

    function add(uint amount) public {
        x += amount;
        y += amount;
    }
}