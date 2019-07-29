pragma solidity >=0.5.0;

contract StructsLocalStorageFunc {
    struct S {
        int x;
    }

    S s1;
    S s2;

    function set_x(S storage s, int _x) internal {
        s.x = _x;
    }

    function() external payable {
        s1.x = 1;
        s2.x = 2;
        S storage sl1 = s1;
        S storage sl2 = s2;
        set_x(sl1, 1);
        set_x(sl2, 2);
        assert(s1.x == 1);
        assert(s2.x == 2);
    }
}