pragma solidity >=0.5.0;

// To be used together with Import2.sol
contract A {
    /** @notice postcondition x == y */
    function f(uint x) public pure returns (uint y) {
        return x;
    }
}