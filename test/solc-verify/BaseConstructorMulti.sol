pragma solidity >=0.5.0;

contract A {
    int public n;
    constructor(int x) public {
        n += x;
    }
}

contract B is A(1) {

}

contract BaseConstructorMulti is A, B {
    constructor() public {
        assert(n == 1); // Constructor of A is only called once
    }

    function() external payable { } // Needed for detecting as a truffle test case
}