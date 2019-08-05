pragma solidity >=0.5.0;

contract Issue021 {
    uint[] a;

    /// @notice postcondition x == 3
    /// @notice postcondition y == 2
    function f() public returns (uint x, uint y) {
        a = [1,2,3];
        return (a[2], [2,3,4][0]);
    }
}
