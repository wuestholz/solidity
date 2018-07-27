pragma solidity ^0.4.23;

contract BitPreciseExt {
    function u32tou40(uint32 x) private pure returns (uint40) {
        uint40 y = x;
        return y;
    }
    function s16tos48(int16 x) private pure returns (int48) {
        int48 y = x;
        return y;
    }

    function __verifier_main() public pure {
        assert(u32tou40(123) == 123);
        assert(s16tos48(123) == 123);
        assert(s16tos48(-123) == -123);
    }
}