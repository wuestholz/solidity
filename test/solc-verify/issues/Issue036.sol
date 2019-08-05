pragma solidity >=0.5.0;

contract C {
    /// @notice postcondition x == -1
    function f() public pure returns(int x) {
        return 2-3;
    }
}