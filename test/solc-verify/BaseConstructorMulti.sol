pragma solidity >=0.5.0;

contract A {
    int public x;
    constructor(int _x) public { x = _x; }
}

contract B is A(1) {

}

contract C is A, B {

}