pragma solidity >=0.5.0;

contract StructsLocalStorage {
    struct S {
        int x;
        T t;
    }

    struct T {
        int z;
    }

    S ss;

    function() external payable {
        ss.x = 1;
        ss.t.z = 2;
        assert(ss.x == 1);
        assert(ss.t.z == 2);
        set(ss);
        assert(ss.x == 3);
        assert(ss.t.z == 4);
        S storage sl = ss;
        sl.x = 5;
        sl.t.z = 6;
        assert(ss.x == 5);
        assert(ss.t.z == 6);
    }

    function set(S storage sl) private {
        sl.x = 3;
        sl.t.z = 4;
    }
}