pragma solidity >=0.5.0;

contract C {
    int x;
    constructor(int _x) public { x = _x; }
}

contract D is C(1) {
    constructor() public {
        assert(x == 1);
    }
}

contract E is C {
    constructor() C(1) public {
        assert(x == 1);
    }
}