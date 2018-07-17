pragma solidity ^0.4.23;

contract PayableFunctions {

    function receive(uint param) public payable returns (uint) {
        return param + msg.value;
    }
}

contract Payable {
    PayableFunctions p;

    function transfer(uint amount) public returns (uint) {
        return p.receive.gas(5000).value(amount)(1);
    }

    function transferNested(uint amount) public returns (uint) {
        return p.receive.value(p.receive.value(1)(2))(p.receive.value(amount)(3));
    }

    function __verifier_main() public {
        assert(transfer(1) == 2);
        assert(transferNested(1) == 7);
    }
}
