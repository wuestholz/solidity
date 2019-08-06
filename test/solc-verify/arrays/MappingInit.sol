pragma solidity ^0.5.0;

/// @notice invariant m[0] == 0
/// @notice invariant nested[0][0] == 0
/// @notice invariant ms[0].x == 0
/// @notice invariant __verifier_eq(m, m2)
contract Test {
    struct S { int x; }

    mapping(int => int) m;
    mapping(int => int) m2;
    mapping(int => mapping(int => int)) nested;
    mapping(int => S) ms;

    constructor() public {
        assert(m[0] == m2[0]);
    }
}