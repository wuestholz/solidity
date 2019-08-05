pragma solidity >=0.5.0;

contract C1 {
    constructor() public {
        // Should hold
        assert(address(this).balance >= 0);
    }
}

contract C2 {
    constructor() public {
        // Might not hold
        assert(address(this).balance == 0);
    }
}