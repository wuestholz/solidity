pragma solidity ^0.4.23;

contract DemoBitPrecise {
    function add8(int8 a, int8 b) public pure returns (int8) {
        require(a >= 0 && b >= 0);
        require(a <= 63 && b <= 63);
        int8 result = a + b;
        assert(result >= 0);
        return result;
    }
}