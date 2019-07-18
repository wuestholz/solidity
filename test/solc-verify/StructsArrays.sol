pragma solidity >=0.5.0;

contract StructsArrays {

  struct A {
    int x;
    int[] m;
  }

  A a1;
  A a2;

  function() external payable {
    A memory ma = A(1, new int[](2));
    ma.m[0] = -1; ma.m[1] = -2;
    assert(a1.x == 0);
    assert(a2.x == 0);
    assert(ma.x == 1);

    a1.m.length = 3;
    ma.x = 2;
    a2.x = 3;

    a1 = ma;
    assert(a1.x == 2);
    assert(a1.m.length == 2);
    assert(a1.m[0] == -1);
    assert(a1.m[1] == -2);

    a1 = a2;
    assert(a1.x == 3);
    assert(a1.m.length == 0);
  }

}