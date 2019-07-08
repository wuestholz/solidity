pragma solidity >=0.5.0;

contract A {
    int public x;
    /// @notice postcondition (x == __verifier_old_int(_x))
    function set(int _x) public { x = _x; }
    constructor(int _x) public { set(_x); }
}

contract B1 is A {
  constructor(int _x) public A(_x) { set(x*_x); }
}

contract B2 is A {
  constructor(int _x) public { set(x+_x); }
}

contract BaseConstructorOrder is B1, B2 {

  modifier m(int _x) {
    _;
    set(_x);
    assert(x == 6);
  }

  constructor() public B1(x+2) m(x+2) B2(x) {
    assert(x == 4);
  }

  function() external payable { } // Needed for detecting as a truffle test case
}