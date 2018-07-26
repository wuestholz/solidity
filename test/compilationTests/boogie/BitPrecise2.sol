pragma solidity ^0.4.23;

contract BitPrecise2 {
    function __verifier_main() public pure {
        uint8 n127bv8 = 127;
        uint8 n128bv8 = 128;
        assert(n127bv8 + n128bv8 == 255);
        assert(n128bv8 + n128bv8 == 0);
        
        uint8 test = 128;
        test += 128;
        assert(test == 0);
    }
}