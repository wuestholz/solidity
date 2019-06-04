pragma solidity >=0.5.0;

contract Enums {
    enum Dir { Up, Down, Left, Right }

    enum Other { A, B, C, D, E }

    Dir dir;

    constructor() public {
        dir = Dir.Up;
    }

    /** @notice postcondition dir == d */
    function setDir(Dir d) public {
        dir = d;
    }

    /** @notice postcondition dir == Dir.Up */
    function goUp() public {
        dir = Dir.Up;
    }

    function conversion(int x) public {
        require(0 <= x && x <= 3);
        dir = Dir(x); // Guarded, cannot fail
    }

    function conversion2(Other other) public {
        dir = Dir(int(other)); // Conversion might fail
    }
}