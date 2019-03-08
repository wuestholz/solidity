pragma solidity ^0.4.23;

/** @notice invariant x == y */
contract C {
    int x;
    int y;

    /** @notice precondition x == y
        @notice postcondition x == (y + n) */
    function add_to_x(int n) internal {
        x = x + n;
        require(x >= y); // Ensures that there is no overflow
    }

    function add(int n) public {
        require(n >= 0);
        add_to_x(n);
        /** @notice invariant y <= x */
        while (y < x) {
            y = y + 1;
        }
    }
}