pragma solidity ^0.4.23;

contract PayableFunctions {

    function receive(uint param) public payable returns (bool) {
        return param < msg.value;
    }
}

contract Other {
    PayableFunctions p;

    function transfer(uint amount) public {
        p.receive.value(amount)(100);
    }
}
