pragma solidity ^0.4.23;

contract Addresses {

    function sendMoney(uint amount) public {
        address myAddress = this;
        address receiver = msg.sender;
        if (myAddress.balance >= amount) receiver.transfer(amount);
    }

    function sendMoney2(uint amount) public {
        if (address(this).balance >= amount) msg.sender.transfer(amount);
    }
}