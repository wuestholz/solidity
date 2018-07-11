pragma solidity ^0.4.23;

contract SomeContract {
    function someFunc() {
        uint x = 10 ** 20 + 1;
        uint y = 100000000000000000001;
        assert(x == y);
    }
}