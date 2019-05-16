pragma solidity >=0.5.0;

contract StructsStorage {
    struct S {
        int x;
        T t;
    }

    struct T {
        int z;
    }

    S s1;
    S s2;

    function() external payable {
        s1.x = 1;
        s2.x = 2;
        s1.t.z = 3;
        s2.t.z = 4;

        s1 = s2;

        S storage sl = s1;

        assert(s1.x == 2);
        assert(s2.x == 2);
        assert(sl.x == 2);
        assert(s1.t.z == 4);
        assert(s2.t.z == 4);
        assert(sl.t.z == 4);

        s1.x = 1;
        s1.t.z = 3;

        assert(s1.x == 1);
        assert(s2.x == 2);
        assert(sl.x == 1);
        assert(s1.t.z == 3);
        assert(s2.t.z == 4);
        assert(sl.t.z == 3);
    }
}