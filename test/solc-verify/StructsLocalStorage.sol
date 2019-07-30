pragma solidity >=0.5.0;

contract StructsLocalStorage {
    struct S {
        int x;
        T t;
    }

    struct T {
        int z;
    }

    T t1;
    S ss;

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

    function testMember() public {
        ss.x = 1;
        ss.t.z = 2;
        t1.z = 3;
        assert(ss.x == 1);
        assert(ss.t.z == 2);
        assert(t1.z == 3);

        T storage tl = ss.t;
        tl.z = 4;

        assert(ss.x == 1);
        assert(ss.t.z == 4);
        assert(t1.z == 3);

        tl = t1;
        tl.z = 5;

        assert(ss.x == 1);
        assert(ss.t.z == 4);
        assert(t1.z == 5);
    }

    function() external payable {
        testSimple();
        testMember();
    }
}