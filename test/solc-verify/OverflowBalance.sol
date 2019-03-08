pragma solidity ^0.4.24;

/**
 * @notice invariant bal == this.balance
 */
contract Overflow {
    uint256 bal;

    constructor() {
        bal = 0;
    }

    function receive() public payable {
        bal += msg.value; // Should not overflow
    }
}