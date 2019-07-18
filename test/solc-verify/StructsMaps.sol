pragma solidity >=0.5.0;

contract StructsMaps {

  struct A {
    int x;
    mapping(address=>int) m;
  }

  A a1;
  A a2;

  function() external payable {
    A memory ma = A(1);
    assert(a1.x == 0);
    assert(a2.x == 0);
    assert(ma.x == 1);

    a1.m[address(this)] = 2;
    ma.x = 2;
    a2.x = 3;

    a1 = ma;
    assert(a1.x == 2);
    assert(a1.m[address(this)] == 2);

    a1 = a2;
    assert(a1.x == 3);
    assert(a1.m[address(this)] == 2);
  }

}