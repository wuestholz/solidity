pragma solidity ^0.4.23;

contract Base {
    function f() public pure returns (uint) {
        return 1;
    }

    function g() public pure returns (uint) {
        return 2;
    }
}

contract Inheritance is Base {
    function f() public pure returns (uint) {
        return 3;
    }

    function h() public pure returns (uint) {
        return f() + g();
    }

    function __verifier_main() public pure {
        assert(h() == 5);
    }
}