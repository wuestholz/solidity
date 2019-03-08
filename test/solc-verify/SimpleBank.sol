pragma solidity ^0.4.23;

/**
 * @notice invariant __verifier_sum_uint256(user_balances) == this.balance
 */
contract SimpleBank {
    mapping(address=>uint) user_balances;

    function deposit() payable public {
        require(this != msg.sender);
        user_balances[msg.sender] += msg.value;
    }

    function withdraw_transfer() public {
        require(this != msg.sender);
        if (user_balances[msg.sender] > 0 && address(this).balance > user_balances[msg.sender]) {
            msg.sender.transfer(user_balances[msg.sender]);
            user_balances[msg.sender] = 0;
        }
    }

    function withdraw_call_incorrect() public {
        require(this != msg.sender);
        uint amount = user_balances[msg.sender];
        if (amount > 0 && address(this).balance > amount) {
            if (!msg.sender.call.value(amount)("")) {
                revert();
            }
            user_balances[msg.sender] = 0;
        }
    }

    function withdraw_call_correct() public {
        require(this != msg.sender);
        uint amount = user_balances[msg.sender];
        if (amount > 0 && address(this).balance > amount) {
            user_balances[msg.sender] = 0;
            if (!msg.sender.call.value(amount)("")) {
                revert();
            }
        }
    }
}
