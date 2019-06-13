pragma solidity >=0.5.0;

contract IntegerExp {

    uint8 constant public decimals = 18;
    uint constant public decimals2 = 256;
    uint totalSupply;

    function() external payable {
        // No overflow
        totalSupply = 7000000000 * (10**(uint256(decimals)));
        assert(totalSupply == 7000000000000000000000000000);
        // Overflow
        uint x = 2 ** decimals2;
        assert(x == 0);
    }
}