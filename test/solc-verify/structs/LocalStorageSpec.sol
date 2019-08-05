pragma solidity >=0.5.0;

contract A {
  struct S {
      int x;
  }
  function setA0(S storage s_ptr) internal {
    s_ptr.x = 0;
  }
}

contract B is A {
  S s;
  /// @notice postcondition s.x == 0
  function setB0(S storage s_ptr) internal {
    setA0(s_ptr);
  }
}

contract LocalStorageSpec is B {
  S s;
  function setC0(S storage s_ptr) internal {
    setB0(s_ptr);
  }
  function() external payable {
    setC0(s);
    assert(s.x == 0);
  }

}