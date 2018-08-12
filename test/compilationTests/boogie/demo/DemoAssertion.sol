pragma solidity ^0.4.23;

contract DemoAssertion {
    function abs(int param) public pure returns (int) {
        int result = param;
        if (result < 0) result *= -1;
        assert(result >= 0);
        return result;
    }
}