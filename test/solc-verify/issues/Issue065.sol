pragma solidity >=0.5.0;

contract Issue065 {
    /// @notice postcondition r == 1
    function value() public pure returns (uint r) {
        return 1;
    }

    function() external payable {
        assert(Issue065(this).value() == 1);
    }
}
