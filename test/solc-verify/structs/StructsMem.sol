pragma solidity >=0.5.0;

contract StructsMem {
    struct S {
        int x;
        T t;
    }

    struct T {
        int z;
    }

    function() external payable {
        // Initialize a new memory struct
        T memory tm = T(2);
        S memory sm = S(1, tm);
        assert(sm.x == 1);
        assert(sm.t.z == 2);

        // Make a reference copy
        S memory sm2 = sm;
        assert(sm.x == 1);
        assert(sm.t.z == 2);
        assert(sm2.x == 1);
        assert(sm2.t.z == 2);

        // Change copy, original also changes
        sm2.x = 3;
        sm2.t.z = 4;
        assert(sm.x == 3);
        assert(sm.t.z == 4);
        assert(sm2.x == 3);
        assert(sm2.t.z == 4);
    }
}