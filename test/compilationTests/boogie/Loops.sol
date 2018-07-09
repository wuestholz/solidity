pragma solidity ^0.4.23;

contract Loops {

    function whileLoopFunc(uint param) pure public returns (uint) {
        uint i = 0;
        uint result = param;

        while (i < 10) {
            result = result + 1;
            i = i + 1;
        }

        return result;
    }

    function forLoopFunc(uint param) pure public returns (uint) {
        uint result = param;

        for (uint i = 0; i < 10; i = i + 1) {
            result = result + 1;
        }

        return result;
    }

    function breakLoop(uint param) pure public returns (uint) {
        uint i = 0;
        uint result = param;

        while (i < 10) {
            result = result + 1;
            if (result >= 100) break;
            i = i + 1;
        }

        return result;
    }

    function __verifier_main() pure public {
        assert(whileLoopFunc(5) == 15);
        assert(forLoopFunc(9) == 19);
        assert(breakLoop(5) == 15);
        assert(breakLoop(95) == 100);
    }

}