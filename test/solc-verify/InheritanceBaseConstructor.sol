pragma solidity >=0.5.0;

/// @notice invariant x == 42
contract C {
    int x;
    constructor() public { x = 42; }
}

/// @notice invariant C.x == 42
contract D is C { }

/// @notice invariant C.x == 42
contract E is D {
    constructor() public { }
}