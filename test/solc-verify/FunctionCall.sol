pragma solidity >=0.5.0;

contract FunctionCall {
    uint x;

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

    function sequentialCall() public {
        uint y = f() + g();
        assert(y == 3);
    }

    function compositeCall() public {
        uint y = h(f() + f());
        assert(y == 7);
    }
}