pragma solidity ^0.4.23;

contract A {
    address owner;
    uint x;

    modifier onlyOwner {
        require(msg.sender == owner);
        _;
        assert(true);
    }

    modifier onlyOwner2 {
        if (msg.sender != owner) {
            return;
        }
        _;
    }

    function someFunc(uint y) public onlyOwner returns (uint) {
        x += y;
        return x;
    }

    function someFunc2(uint y) public onlyOwner2 returns (uint) {
        x += y;
        return x;
    }
}