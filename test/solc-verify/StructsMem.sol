pragma solidity >=0.5.0;

contract StructsMem {
    struct S {
        int x;
        bool y;
        T t;
    }

    struct T {
        int z;
    }

    function () external {
        T memory tm = T(2);
        S memory sm = S(1, true, tm);
        assert(sm.t.z == 2);

        S memory sm2 = sm;
        sm2.t.z = 5;
        assert(sm.t.z == 5);
    }
}