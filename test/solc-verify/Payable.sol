pragma solidity >=0.5.0;

contract PayableFunctions {

    /**
     * @notice postcondition r == param + msg.value
     */
    function receive(uint param) public payable returns (uint r) {
        return param + msg.value;
    }
}

contract Payable {
    PayableFunctions p;

    function transfer(uint amount) private returns (uint) {
        require(address(this).balance >= amount);
        return p.receive.gas(5000).value(amount)(1);
    }

    function __verifier_main() public {
        assert(transfer(1) == 2);
    }

    function transferNested(uint amount) public returns (uint) {
        require(amount >= 0);
        require(address(this).balance >= amount + 3);
        // Calling a payable function multiple times with checking for balance
        // only in the beginning will fail, because the called function might
        // modify our balance as well. However, this is an expected failure and
        // currently we are not reporting it.
        return p.receive.value(p.receive.value(1)(2))(p.receive.value(amount)(3));
    }
}
