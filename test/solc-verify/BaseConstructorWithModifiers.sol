pragma solidity >=0.5.0;

contract A {
    int public x;

    modifier m(int _m) {
        x += _m;
        _;
    }

    constructor(int _x) public m(_x) { x++; }
}

contract BaseConstructorWithModifiers is A {

    constructor() A(1) m(2) public {
        assert(x == 4);
    }

    function() external payable { } // Needed for detecting as a truffle test case
}