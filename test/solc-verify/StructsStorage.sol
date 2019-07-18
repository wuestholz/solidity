pragma solidity >=0.5.0;

contract StructsStorage {
    struct S {
        int x;
        T t;
    }

    struct T {
        int z;
    }

    // Two instances and a mapping, should be different
    S s1;
    S s2;
    mapping(int=>S) ss;

    function() external payable {
        // Set values, no aliasing, asserts should hold
        s1.x = 1;
        s2.x = 2;
        s1.t.z = 3;
        s2.t.z = 4;
        ss[0].x = 5;
        ss[1].x = 6;
        ss[0].t.z = 7;
        ss[1].t.z = 8;

        assert(s1.x == 1);
        assert(s2.x == 2);
        assert(s1.t.z == 3);
        assert(s2.t.z == 4);
        assert(ss[0].x == 5);
        assert(ss[1].x == 6);
        assert(ss[0].t.z == 7);
        assert(ss[1].t.z == 8);

        // Deep copy, asserts should hold
        s1 = ss[1];
        ss[0] = s2;

        assert(s1.x == 6);
        assert(s2.x == 2);
        assert(s1.t.z == 8);
        assert(s2.t.z == 4);
        assert(ss[0].x == 2);
        assert(ss[1].x == 6);
        assert(ss[0].t.z == 4);
        assert(ss[1].t.z == 8);

        // Change should be local, asserts should hold
        s1.x = 1;
        s1.t.z = 3;
        ss[0].x = 5;
        ss[0].t.z = 7;

        assert(s1.x == 1);
        assert(s2.x == 2);
        assert(s1.t.z == 3);
        assert(s2.t.z == 4);
        assert(ss[0].x == 5);
        assert(ss[1].x == 6);
        assert(ss[0].t.z == 7);
        assert(ss[1].t.z == 8);

        // Copy only inner structure
        s1.t = ss[0].t;

        assert(s1.x == 1);
        assert(s2.x == 2);
        assert(s1.t.z == 7);
        assert(s2.t.z == 4);
        assert(ss[0].x == 5);
        assert(ss[1].x == 6);
        assert(ss[0].t.z == 7);
        assert(ss[1].t.z == 8);
    }
}