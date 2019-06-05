pragma solidity >=0.5.0;

contract EnumsBv {
    enum Dir { Up, Down, Left, Right }

    function ok() public pure {
        int8 x = 3;
        Dir d = Dir(x);
        assert(d == Dir.Right);
    }

    function ok2() public pure {
        int8 x = int8(Dir.Down);
        assert(x == 1);
    }

    function fail() public pure {
        int8 x = 5;
        Dir d = Dir(x);
    }
}