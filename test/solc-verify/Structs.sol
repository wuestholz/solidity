pragma solidity >=0.5.0;
pragma experimental ABIEncoderV2;

contract Structs {

  struct S1 {
    int x;
    bool flag;
  }

  struct S2 {
    mapping (address => S1) m;
    bool flag;
  }

  mapping (address => S2) m;

  S1 f_s1;
  S1 f_s1_other;

  constructor() public {
    // Initialize something
    S2 storage owner_entry = m[msg.sender];
    owner_entry.flag = true;
    owner_entry.m[msg.sender] = S1(1, true);
  }

  function setS1_mem(S1 memory s1_mem) public {
    f_s1 = s1_mem;
  }

  function setS1_stor(S1 storage s1_stor) internal {
    f_s1_other = s1_stor;
  }

  function() external payable {
    // Copy from memory to storage
    S1 memory s1_mem = S1(1, true);
    f_s1 = s1_mem;
    assert(f_s1.x == s1_mem.x);
    assert(f_s1.flag == s1_mem.flag);

    // Two storage, should be reference assignment
    f_s1_other = f_s1;

    // Set by memory (copy values from f_s1_other to f_s1)
    setS1_mem(f_s1_other);
    assert(f_s1.flag == f_s1_other.flag);
    f_s1.flag = false;
    f_s1_other.flag = true;
    assert(f_s1.flag != f_s1_other.flag);

    // Set by storage (here references f_s1_other = f_s1)
    S1 storage s1_mine = m[msg.sender].m[msg.sender];
    setS1_stor(s1_mine);
    f_s1 = s1_mine;
    assert(f_s1.flag == f_s1_other.flag);
    f_s1.flag = false;
    f_s1_other.flag = true;
    assert(f_s1.flag != f_s1_other.flag);
  }


}
