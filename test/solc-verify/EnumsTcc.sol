pragma solidity >=0.5.0;

contract Enums {
    enum Dir { Up, Down, Left, Right }

    function f(Dir d) public pure {
        int x = int(d);
        assert(0 <= x && x < 4);
    }
}