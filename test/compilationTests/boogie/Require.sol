pragma solidity ^0.4.23;

contract Assertion {
    function add(int x, int y) pure public returns (int) {
        require(x > 0);
        require(y > 0);
        int result = x + y;
        assert(result > x);
        assert(result > y);
        return result;
    }
}