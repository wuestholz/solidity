pragma solidity ^0.4.24;

/**
 * @notice invariant x == y
 */
contract Overflow {
 
    uint8 x;
    uint8 y;

    function inc_overflow() public {
        x = x + 1;
        y = y + 1;
    }

    function inc_no_overflow() public {
        x = x + 1;
        require(x > y);
        y = y + 1;
        // No require needed for y, the one for x and the invariant is sufficient
    }
}