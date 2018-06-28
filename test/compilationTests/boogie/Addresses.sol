pragma solidity ^0.4.23;

contract Addresses {
    function checkBalance(uint x) view public returns (bool) {
        address myAddress = this;
        if (myAddress.balance > x) return true;
        return false;
    }

    function sendMoney(address receiver, uint amount) public {
        address myAddress = this;
        if (myAddress.balance >= amount) receiver.transfer(amount);
    }
}