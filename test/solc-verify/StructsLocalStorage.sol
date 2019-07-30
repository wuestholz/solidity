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
    S s1;
    S s2;

    function testSimple() public {
        s1.x = 1;
        s1.t.z = 2;
        assert(s1.x == 1);
        assert(s1.t.z == 2);

        S storage sl = s1;
        assert(sl.x == 1);
        assert(sl.t.z == 2);

        s1.x = 3;
        s1.t.z = 4;
        assert(sl.x == 3);
        assert(sl.t.z == 4);

        sl.x = 5;
        sl.t.z = 6;
        assert(s1.x == 5);
        assert(s1.t.z == 6);
    }

    function testMember() public {
        s1.x = 1;
        s1.t.z = 2;
        t1.z = 3;
        assert(s1.x == 1);
        assert(s1.t.z == 2);
        assert(t1.z == 3);

        T storage tl = s1.t;
        tl.z = 4;

        assert(s1.x == 1);
        assert(s1.t.z == 4);
        assert(t1.z == 3);

        tl = t1;
        tl.z = 5;

        assert(s1.x == 1);
        assert(s1.t.z == 4);
        assert(t1.z == 5);
    }

    function testConditional(bool b) public {
        // Set three members to 1
        t1.z = 1;
        s1.t.z = 1;
        s2.t.z = 1;
        // Conditionally point to one of the first two
        T storage ptr = t1;
        if (b) ptr = s1.t;
        ptr.z = 2; // Change

        // The third one should not change regardless condition
        assert(s2.t.z == 1);
    }

    function() external payable {
        testSimple();
        testMember();
        testConditional(true);
        testConditional(false);
    }
}