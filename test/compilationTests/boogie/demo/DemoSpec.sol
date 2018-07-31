pragma solidity ^0.4.23;

/**
 * @notice invariant x == y
 */
contract DemoSpec {
    uint x = 0;
    uint y = 0;

    function add(uint amount) public {
        x += amount;
        y += amount;
    }
}