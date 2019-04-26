pragma solidity >=0.5.0;

contract Tuples {
   
    function minMax(int x, int y) private pure returns (int min, int max) {
      if (x > y) {
        min = y;
        max = x;
      } else {
        min = x;
        max = y;
      }
    }

    function() external payable {
      int x = 10;
      int y = 20;
      (int min, int max) = minMax(x, y);
      assert(min == 10 && max == 20);
    }
    
}
