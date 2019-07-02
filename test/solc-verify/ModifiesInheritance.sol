pragma solidity >=0.5.0;

contract C {
    int x;
    /// @notice modifies x
    function f() public { x = 0; }
}

contract D is C {
    int x;

    /// @notice modifies C.x
    function correct() public { f(); }

    function incorrect1() public { f(); }

    function incorrect2() public { x = 3; }
}