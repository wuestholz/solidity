pragma solidity ^0.4.17;

contract FunctionCall {
    uint x;

    function set(uint x1) private {
        x = x1;
    }

    function get() view public returns (uint) {
        return x;
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
        return h1 + 5;
    }

    function sequentialCall() public {
        x = f() + g();
    }

    function compositeCall() public {
        x = h(f() + f());
    }

    function __verifier_main() public {
        set(5);
        assert(get() == 5);
        addSome();
        assert(get() == 6);
        sequentialCall();
        assert(get() == 3);
        compositeCall();
        assert(get() == 7);
    }

}