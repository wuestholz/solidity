pragma solidity ^0.5.0;

contract Test {
    mapping(int => int) m;

    /// @notice postcondition m[0] == 1
    function f() public {
        m[0] = 1;
    }
}