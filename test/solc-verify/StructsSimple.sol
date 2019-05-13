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

    function test() public {
        s1.x = 5;
        s1.t.z = 8;
        assert(s1.x == 5);
        assert(s1.t.z == 8);
    }
}