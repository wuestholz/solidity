pragma solidity >=0.5.0;

contract C {
    int x;
    /// @notice modifies x
    function f() public { x = 0; }
}

contract D is C {
    int x;

    /// @notice modifies C.x
    function correct1() public { f(); }

    /// @notice modifies x
    function correct2() public { x = 5; }

    function incorrect1() public { f(); }

    function incorrect2() public { x = 3; }

    /// @notice modifies x
    function incorrect3() public { C.x = 3; }
}