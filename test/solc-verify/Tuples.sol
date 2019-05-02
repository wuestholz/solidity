pragma solidity >=0.5.0;

contract Tuples {

    function testPartial() public pure {
      int x = 2;
      int y = 3;

      (x,) = (1, 1);
      assert(x == 1 && y == 3);
      (,y) = (x, x+1);
      assert(x == 1 && y == 2);
      (y,) = minMax(x, y);
      assert(x == 1 && y == 1);
      (int min,) = minMax(1, 100);
      (,int max) = minMax(1, 100);
      assert(min == 1 && max == 100);
    }

    function testSideffect() public pure {
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
    }

    function testCasts() public pure {
      uint8 x8 = 0;
      uint16 y16 = 0;
      uint32 x32 = 0;
      uint64 y64 = 0;
      (x8, y16) = (1, 2);
      assert(x8 < y16);
      (x32, y64) = (x8 + y16, y16);
      assert(x32 > y64);
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
      testCasts();
      testPartial();
    }
    
}
