pragma solidity >=0.5.0;

contract A {
    int public x;
    int public n;
    constructor(int _x) public {
        x = _x;
        n++;
    }
}

contract B is A(1) {

}

contract BaseConstructorMulti is A, B {
    function() external payable {
        assert(n == 1);
    }
}