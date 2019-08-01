pragma solidity >=0.5.0;

contract Partial {

    /// @notice postcondition r == x + 1
    function cannotverify(uint x) public pure returns (uint r) {
        assembly {}
        return x + 1;
    }

    function f() public pure {
        uint x = cannotverify(5);
        assert(x == 6);
    }
}