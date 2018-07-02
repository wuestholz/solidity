pragma solidity ^0.4.23;

contract Addresses {

    function sendMoney(uint amount) public {
        address myAddress = this;
        address receiver = msg.sender;
        if (myAddress.balance >= amount) receiver.transfer(amount);
    }

    function sendMoney2(uint amount) public {
        uint oldSum = address(this).balance + msg.sender.balance;
        if (address(this).balance >= amount) msg.sender.transfer(amount);
        uint newSum = address(this).balance + msg.sender.balance;
        assert(oldSum == newSum);
    }

    function sendMoneyError(uint amount) public {
        uint oldSum = address(this).balance + msg.sender.balance;
        if (address(this).balance >= amount) msg.sender.transfer(amount);
        uint newSum = address(this).balance + msg.sender.balance;
        assert(oldSum == (newSum + 1234)); // This assertion should fail
    }
}