pragma solidity ^0.4.23;

contract Initializers {
    int b;
    int c;

    constructor(int b0) public {
        b = b0;
    }

    function getA() public view returns (int) {
        return a;
    }
    int a = 5;
}

contract NoConstructorButNeeded {
    int a = 5;
    int b;
}

contract NoConstructorNeeded {
    int a;
    int b;
}