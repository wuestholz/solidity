pragma solidity ^0.4.23;

/**
 * @notice invariant __verifier_sum_int32(user_balances) == total_supply
 */
contract SomeContract {
    int32 total_supply;
    mapping(address=>int32) user_balances;

    function deposit(int32 amount) public {
        total_supply += amount;
        user_balances[msg.sender] += amount;
    }

    function withdraw() public {
        total_supply -= user_balances[msg.sender];
        user_balances[msg.sender] = 0;
    }

    function withdraw_incorrect() public {
        total_supply -= user_balances[msg.sender];
        user_balances[msg.sender] = 12345;
    }
}
