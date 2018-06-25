pragma solidity ^0.4.17;

contract IfElse {
    uint contractVar;

    function doSomething(uint param) view public returns (uint) {
        if (param < 10) {
            return contractVar + 10;
        } else if (param < 20) {
            return contractVar + 20;
        } else {
            return contractVar;
        }
    }
}