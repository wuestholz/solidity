pragma solidity ^0.4.17;

contract Assertion {

    function abs_sound(int param) pure public returns (int) {
        int result = param;
        if (result < 0) result *= -1;
        assert(result >= 0);
        return result;
    }

    function abs_unsound(int param) pure public returns (int) {
        int result = param;
        if (result < 0) result += -1;
        assert(result >= 0);
        return result;
    }
}