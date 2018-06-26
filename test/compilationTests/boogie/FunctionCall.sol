pragma solidity ^0.4.17;

contract FunctionCall {
    uint x;

    function set(uint x1) private {
        x = x1;
    }

    function addSome() public {
        set(x + 1);
    }

    function f() pure public returns (uint) {
        return 1;
    }

    function g() pure public returns (uint) {
        return 2;
    }

    function h(uint h1) pure public returns (uint) {
        return h1;
    }

    function sequentialCall() public {
        x = f() + g();
    }

    function compositeCall() public {
        x = h(f());
    }

}