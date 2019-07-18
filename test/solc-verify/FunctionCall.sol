pragma solidity >=0.5.0;

contract FunctionCall {
    uint x;
    uint y;
    uint a;

    function set(uint x1) private {
        x = x1;
    }

    function get() view private returns (uint) {
        return x;
    }

    function addSome() public {
        uint old_x = x;
        set(x + 1);
        assert(x == old_x + 1);
    }

    /**
     * @notice postcondition r == 1
     */
    function f() pure public returns (uint r) {
        return 1;
    }

    /**
     * @notice postcondition r == 2
     */
    function g() pure public returns (uint r) {
        return 2;
    }

    /**
     * @notice postcondition r == h1 + 5
     */
    function h(uint h1) pure public returns (uint r) {
        return h1 + 5;
    }

    function sequentialCall() pure public {
        uint y1 = f() + g();
        assert(y1 == 3);
    }

    function compositeCall() pure public {
        uint y1 = h(f() + f());
        assert(y1 == 7);
    }

    function add(uint x1, uint y1) private pure returns (uint sum) {
        sum = x1 + y1;
    }

    function() external payable {
      a = 0;
      x = 0;
      y = 0;
      uint sum = add(a ++ /* 0 */, (++ x) + a /** 2 */);
      assert(a == 1);
      assert(x == 1);
      assert(sum == 2);
    }

}