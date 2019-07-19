pragma solidity >=0.5.0;

/// @notice invariant __verifier_sum_uint(balances) <= address(this).balance
contract SimpleMultiBankCorrect {

    mapping(address=>uint) balances;

    function deposit() public payable {
        balances[msg.sender] += msg.value;
        require(balances[msg.sender] >= msg.value);
    }

    function multiSend(uint amount, address payable[] memory recipients) public {
        uint n = recipients.length;
        require(0 < n && n <= 20);
        uint totalAmount = amount * n;
        require(totalAmount/n == amount);
        require(balances[msg.sender] >= totalAmount);
        balances[msg.sender] -= totalAmount;
        /// @notice invariant i <= n
        /// @notice invariant __verifier_sum_uint(balances) + (n-i)*amount <= address(this).balance
        for (uint i = 0; i < n; ++ i) {
            address payable recipient = recipients[i];
            (bool ok, ) = recipient.call.value(amount)(""); // No reentrancy attack
            if (!ok) revert();
        }
    }
}
