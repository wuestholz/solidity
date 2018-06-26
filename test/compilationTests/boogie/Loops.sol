pragma solidity ^0.4.17;

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
}