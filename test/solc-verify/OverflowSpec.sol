pragma solidity >=0.5.0;

/**
 * @notice invariant x == y + 1
 */
contract Overflow {
    uint8 x = 1;
    uint8 y = 0;

    function inc() public {
        x++;
        y++;
    } // Overflow in spec

    function set() public {
        x = 0;
        y = 255;
    } // No overflow in code, but overflow in spec
}