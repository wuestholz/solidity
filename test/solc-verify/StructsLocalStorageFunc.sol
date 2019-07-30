pragma solidity >=0.5.0;

contract StructsLocalStorageFunc {
    struct S {
        int x;
    }

    struct T {
        S st1;
        S st2;
    }

    S s1;
    S s2;
    T t;

    function set_x(S storage s, int _x) internal {
        s.x = _x;
    }

    function() external payable {
        s1.x = 1;
        s2.x = 2;
        S storage sl1 = s1;
        S storage sl2 = s2;
        S storage sl3 = t.st2;
        set_x(sl1, 1);
        set_x(sl2, 2);
        set_x(sl3, 3);
        assert(s1.x == 1);
        assert(s2.x == 2);
        assert(t.st2.x == 3);
    }
}