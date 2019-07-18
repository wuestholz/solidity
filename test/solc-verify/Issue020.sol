pragma solidity >=0.5.0;

contract Issue020 {
  function() external payable {
    uint d1 = 654_321;
    uint d2 =  54_321;
    uint d3 =   4_321;
    uint d4 = 5_43_21;
    uint d5 = 1_2e10;
    uint d6 = 12e1_0;

    assert(d1 == 654321);
    assert(d2 == 54321);
    assert(d3 == 4321);
    assert(d4 == 54321);
    assert(d5 == 12e10);
    assert(d6 == 12e10);
  }
}
