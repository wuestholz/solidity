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
    S ss2;

    function testSimple() public {
        ss.x = 1;
        ss.t.z = 2;
        assert(ss.x == 1);
        assert(ss.t.z == 2);

        S storage sl = ss;
        assert(sl.x == 1);
        assert(sl.t.z == 2);

        ss.x = 3;
        ss.t.z = 4;
        assert(sl.x == 3);
        assert(sl.t.z == 4);

        sl.x = 5;
        sl.t.z = 6;
        assert(ss.x == 5);
        assert(ss.t.z == 6);
    }

    function() external payable {
        testSimple();
    }
}