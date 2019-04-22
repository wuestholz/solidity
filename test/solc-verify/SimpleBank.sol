pragma solidity >=0.5.0;

/**
 * @notice invariant __verifier_sum_uint256(user_balances) == address(this).balance
 */
contract SimpleBank {
    mapping(address=>uint) user_balances;

    function deposit() payable public {
        require(address(this) != msg.sender);
        user_balances[msg.sender] += msg.value;
    }

    function withdraw_transfer() public {
        require(address(this) != msg.sender);
        if (user_balances[msg.sender] > 0 && address(this).balance > user_balances[msg.sender]) {
            msg.sender.transfer(user_balances[msg.sender]);
            user_balances[msg.sender] = 0;
        }
    }

    function withdraw_call_incorrect() public {
        require(address(this) != msg.sender);
        uint amount = user_balances[msg.sender];
        if (amount > 0 && address(this).balance > amount) {
            (bool ok,) = msg.sender.call.value(amount)("");
            if (!ok) {
                revert();
            }
            user_balances[msg.sender] = 0;
        }
    }

    function withdraw_call_correct() public {
        require(address(this) != msg.sender);
        uint amount = user_balances[msg.sender];
        if (amount > 0 && address(this).balance > amount) {
            user_balances[msg.sender] = 0;
            (bool ok,) = msg.sender.call.value(amount)("");
            if (!ok) {
                revert();
            }
        }
    }
}
