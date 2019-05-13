pragma solidity >=0.5.0;

contract StructsSimple {
    struct S {
        int x;
        bool y;
        T t;
    }

    struct T {
        int z;
    }

    S s1;

    function () external {
        s1.x = 5;
        s1.t.z = 8;
        assert(s1.x == 5);
        assert(s1.t.z == 8);

        T memory tm = T(2);
        S memory sm = S(1, true, tm);
        assert(sm.t.z == 2);
    }
}