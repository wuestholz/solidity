pragma solidity ^0.4.23;

contract Addresses {

    function testTransfer(uint amount) public {
        address myAddress = this;
        address receiver = msg.sender;
        if (myAddress.balance >= amount) receiver.transfer(amount);
    }

    function testTransfer2(uint amount) public {
        uint oldSum = address(this).balance + msg.sender.balance;
        if (address(this).balance >= amount) msg.sender.transfer(amount);
        uint newSum = address(this).balance + msg.sender.balance;
        assert(oldSum == newSum);
    }

    function testTransferError(uint amount) public {
        uint oldSum = address(this).balance + msg.sender.balance;
        if (address(this).balance >= amount) msg.sender.transfer(amount);
        uint newSum = address(this).balance + msg.sender.balance;
        assert(oldSum == (newSum + 1234)); // This assertion should fail
    }

    function testSend(uint amount) public {
        uint oldSum = address(this).balance + msg.sender.balance;
        if (address(this).balance >= amount) msg.sender.send(amount);
        uint newSum = address(this).balance + msg.sender.balance;
        assert(oldSum == newSum);
    }

    function testSend2(uint amount) public {
        uint oldThisBalance = address(this).balance;
        uint oldSenderBalance = msg.sender.balance;

        bool success = false;
        if (address(this).balance >= amount) success = msg.sender.send(amount);
        
        if (!success) {
            assert(oldThisBalance == address(this).balance);
            assert(oldSenderBalance == msg.sender.balance);
        } else {
            assert(oldThisBalance + oldSenderBalance == address(this).balance + msg.sender.balance);
        }
    }

    function testSendError(uint amount) public {
        uint oldSum = address(this).balance + msg.sender.balance;
        if (address(this).balance >= amount) msg.sender.send(amount);
        uint newSum = address(this).balance + msg.sender.balance;
        assert(oldSum == (newSum + 1234)); // This assertion should fail
    }

    function testCall(address addr, uint param) public returns (bool){
        return addr.call(param);
    }
}