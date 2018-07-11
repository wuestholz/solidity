pragma solidity ^0.4.23;

contract SomeContract {
    uint public stateVar;

    function someFunc() public view returns (uint) {
        return stateVar;
    }
}

contract OtherContract {
    function otherFunc(address addr) public view returns (uint) {
        return SomeContract(addr).someFunc();
    }
}