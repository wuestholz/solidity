pragma solidity >=0.5.0;

contract A {
    int x = 1;
    int y;
}

contract B is A {
    int z = 3;
    int t;

    constructor() public {
        assert(x == 1);
        assert(y == 0);
        assert(z == 3);
        assert(t == 0);
    }
}