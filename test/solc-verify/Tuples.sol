pragma solidity >=0.5.0;

contract Tuples {

    function testSideffect() public pure returns (bool) {
      int a = 0;
      int x = 0;
      int y = 0;
      (x, y) = (++ a /* 1 */, (++ x) + a /** 2 */);
      // Not correct:
      // x = ++ a;
      // y = x + a;
      assert(a == 1);
      assert(x == 1);
      assert(y == 2);
      return true;
    }

    function minMax(int x, int y) private pure returns (int min, int max) {
      if (x > y) {
        min = y;
        max = x;
      } else if (x < y) {
        min = x;
        max = y;
      } else {
        return (0, 0);
      }
    }

    function() external payable {
      int x = 10;
      int y = 20;
      (int min, int max) = minMax(x, y);
      assert(min == 10 && max == 20);
      testSideffect();
    }
    
}
