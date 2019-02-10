pragma solidity ^0.4.23;

/**
 * @notice invariant __verifier_sum_uint256(balances) == this.balance
 */
contract SimpleBank {
    mapping(address=>uint) balances;

    function deposit() public payable {
        balances[msg.sender] += msg.value;
    }

    function withdraw(uint256 amount) public {
        require(balances[msg.sender] > amount);
        if (!msg.sender.call.value(amount)("")) { // Reentrancy attack
            revert();
        }
        balances[msg.sender] -= amount;
    }
}
