pragma solidity >=0.5.0;

contract C {
    int x;
    /// @notice postcondition C.x == 0
    function f() public { x = 0; }
}