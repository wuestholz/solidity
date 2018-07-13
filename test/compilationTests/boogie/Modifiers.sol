pragma solidity ^0.4.23;

contract A {
    address owner;
    uint x;

    modifier onlyOwner {
        require(msg.sender == owner);
        _;
        assert(msg.sender == owner);
    }

    modifier onlyOwner2 {
        if (msg.sender != owner) {
            return;
        }
        _;
        assert(msg.sender == owner);
    }

    function someFunc(uint y) public onlyOwner returns (uint) {
        x += y;
        return x;
    }

    function someFunc2(uint y) public onlyOwner2 returns (uint) {
        x += y;
        return x;
    }

    function twoModifiers(uint y) public onlyOwner onlyOwner2 returns (uint) {
        x += y;
        return x;
    }
}