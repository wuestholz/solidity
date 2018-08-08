pragma solidity ^0.4.23;

contract BitPreciseCast {

    function explicitCasts() public pure {
        uint8 x = 255;
        assert(int8(x) == -1);
        uint16 y = 257;
        assert(uint8(y) == 1);
        int8 z = -3;
        assert(uint8(z) == 253);
    }
}