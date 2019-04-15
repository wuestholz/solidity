pragma solidity >=0.5.0;

contract Casts {
    function literalCast() private pure returns (uint256) {
        // In bit-precise mode, 123 should become 123bv256
        // Otherwise the cast should just be omitted
        uint256 x = uint256(123);
        return x;
    }

    function __verifier_main() public pure {
        assert(literalCast() == 123);
    }
}