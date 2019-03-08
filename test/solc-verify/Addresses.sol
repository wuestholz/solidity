pragma solidity >=0.5.0;

contract Addresses {
    
    function thisBalance(uint amount) public view returns (bool) {
        // 'this.balance' is deprecated but some older contracts use it
        return address(this).balance > amount;
    }

    function testTransfer(uint amount) public {
        uint oldSum = address(this).balance + msg.sender.balance;
        if (address(this).balance >= amount) msg.sender.transfer(amount);
        uint newSum = address(this).balance + msg.sender.balance;
        // We can only state an assertion on the sum, because
        // the sender and the receiver might be the same address
        assert(oldSum == newSum);
    }

    function testTransfer2(uint amount) public {
        require(address(this) != msg.sender); // We require the sender and receiver to be different
        uint oldThisBalance = address(this).balance;
        uint oldSenderBalance = msg.sender.balance;

        if (address(this).balance >= amount) {
            msg.sender.transfer(amount);
            // We can state a stronger assertion because sender is different than receiver
            assert(oldThisBalance - amount == address(this).balance);
            assert(oldSenderBalance + amount == msg.sender.balance);
        }
        
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
        
        if (success) {
            assert(oldThisBalance + oldSenderBalance == address(this).balance + msg.sender.balance);
        } else {
            assert(oldThisBalance == address(this).balance);
            assert(oldSenderBalance == msg.sender.balance);
        }
    }

    function testSend3(uint amount) public {
        require(address(this) != msg.sender); // We require the sender and receiver to be different
        uint oldThisBalance = address(this).balance;
        uint oldSenderBalance = msg.sender.balance;

        bool success = false;
        if (address(this).balance >= amount) success = msg.sender.send(amount);
        
        // We can state stronger assertions because sender is different than receiver
        if (success) {
            assert(oldThisBalance - amount == address(this).balance);
            assert(oldSenderBalance + amount == msg.sender.balance);
        } else {
            assert(oldThisBalance == address(this).balance);
            assert(oldSenderBalance == msg.sender.balance);
        }
    }

    function testSendError(uint amount) public {
        uint oldSum = address(this).balance + msg.sender.balance;
        if (address(this).balance >= amount) msg.sender.send(amount);
        uint newSum = address(this).balance + msg.sender.balance;
        assert(oldSum == (newSum + 1234)); // This assertion should fail
    }

    function testCall(address addr) public returns (bool){
        (bool success, bytes memory returnData) = addr.call("");
        return success;
    }

    function literals() public pure {
        address a1 = 0xf17f52151EbEF6C7334FAD080c5704D77216b732;
        address a2 = 0x627306090abaB3A6e1400e9345bC60c78a8BEf57;
        address a3 = 0xf17f52151EbEF6C7334FAD080c5704D77216b732;

        assert(a1 == a3);
        assert(a1 != a2);
    }
}