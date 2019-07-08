pragma solidity >=0.5.0;

contract A {
    int public x;
    constructor(int _x) public { x = _x; }
}

contract B1 is A {
  constructor(int _x) public A(_x) { x += _x; }
}

contract B2 is A { 
  constructor(int _x) public { x = _x; }
}

contract BaseConstructorOrder is B2, B1 {

    constructor() public B1(2) B2(2) {
        assert(x == 4);
    }

    function() external payable { } // Needed for detecting as a truffle test case
}