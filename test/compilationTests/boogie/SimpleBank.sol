pragma solidity ^0.4.23;

/**
 * @notice invariant __verifier_sum(user_balances) == this.balance
 */
contract SimpleBank {
    mapping(address=>uint) user_balances;

    function deposit() payable public {
        require(this != msg.sender);
        user_balances[msg.sender] += msg.value;
    }

    function withdraw() public {
        require(this != msg.sender);
        if (user_balances[msg.sender] > 0 && address(this).balance > user_balances[msg.sender]) {
            msg.sender.transfer(user_balances[msg.sender]);
            user_balances[msg.sender] = 0;
        }
    }
}
