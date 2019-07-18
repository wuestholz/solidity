pragma solidity >=0.5.0;

/**
 * @notice invariant bal <= address(this).balance
 */
contract Overflow {
    uint256 bal;

    constructor() public {
        bal = 0;
    }

    function receive() public payable {
        bal += msg.value; // Should not overflow
    }
}