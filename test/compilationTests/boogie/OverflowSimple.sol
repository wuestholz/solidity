pragma solidity ^0.4.24;

/**
 * @notice invariant x == y
 */
contract OverflowSimple {
 
    uint8 x;
    uint8 y;

    function inc_overflow() public {
        x = x + 1;
        y = y + 1;
    } // Overflow at end

    function inc_no_overflow() public {
        x = x + 1;
        require(x > y);
        y = y + 1;
        // No require needed for y, the one for x and the invariant is sufficient
    }

    function inc_call_overflow() public {
        x = x + 1;
        nothing(); // Overflow before call
        require(x > y);
        y = y + 1;
    }
    function inc_call_no_overflow() public {
        x = x + 1;
        require(x > y);
        nothing(); // OK
        y = y + 1;
    }

    function nothing() private {}
}