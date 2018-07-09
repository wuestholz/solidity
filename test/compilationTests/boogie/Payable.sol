pragma solidity ^0.4.23;

contract PayableFunctions {

    function receive(uint param) public payable returns (uint) {
        return param + msg.value;
    }
}

contract Other {
    PayableFunctions p;

    function transfer(uint amount) public returns (uint) {
        return p.receive.gas(1).value(amount)(100);
    }

    function transferNested(uint amount) public returns (uint) {
        return p.receive.value(p.receive.value(100)(200))(p.receive.value(amount)(300));
    }

    function __verifier_main() public {
        assert(transfer(1) == 101);
        assert(transferNested(1) == 601);
    }
}
