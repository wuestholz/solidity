pragma solidity >=0.5.0;

contract IntegerExp {

    uint8 constant public decimals = 18;
    uint totalSupply;

    constructor() public {
      totalSupply = 7000000000 * (10**(uint256(decimals)));
    }
}