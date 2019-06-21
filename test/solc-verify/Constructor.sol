pragma solidity >=0.5.0;

contract Constr {
    uint value;

    struct S1 {
        uint x;
        uint y;
    }

    struct S2 {
        S1 s1;
        S1 s2;
    }

    S2 s;

    constructor(uint init) public {
        // Value should be initialize to 0 here
        assert(value == 0);
        value = init;
        assert(value == init);
        assert(s.s1.x == 0);
        assert(s.s1.y == 0);
        assert(s.s2.x == 0);
        assert(s.s2.y == 0);
    }

    function doSomething(uint param) public returns (uint) {
        value += param;
        return value;
    }
}