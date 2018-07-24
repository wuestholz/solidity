pragma solidity ^0.4.23;

/**
 * @notice invariant x == y
 */
contract SpecificationDefaultConstructor {
    // The generated default constructor (setting the initial values) should fail
    uint x = 0;
    uint y = 1;

    function add(uint amount) public {
        x += amount;
        y += amount;
    }
}