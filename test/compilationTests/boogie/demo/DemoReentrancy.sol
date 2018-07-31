pragma solidity ^0.4.23;

/**
 * @notice invariant __verifier_sum(user_balances) == this.balance
 */
contract DemoReentrancy {
    mapping(address=>uint) user_balances;

    function deposit() payable public {
        require(this != msg.sender);
        user_balances[msg.sender] += msg.value;
    }

    function withdraw() public {
        require(this != msg.sender);
        uint amount = user_balances[msg.sender];
        if (amount > 0 && address(this).balance > amount) {
            if (!msg.sender.call.value(amount)("")) {
                revert();
            }
            user_balances[msg.sender] = 0;
        }
    }
}
