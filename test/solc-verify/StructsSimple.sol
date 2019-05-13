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
}