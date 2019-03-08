pragma solidity ^0.4.23;

contract BitPrecise {
    function add128(uint128 a, uint128 b) public pure returns (uint128) {
        uint128 result = a + b;
        assert(result >= a); // Can fail
        assert(result >= b); // Can fail
        return result;
    }

    function add8(uint8 a, uint8 b) public pure returns (uint8) {
        require(a <= 127);
        require(b <= 127);
        uint8 result = a + b;
        assert(result >= a);
        assert(result >= b);
        return result;
    }
}