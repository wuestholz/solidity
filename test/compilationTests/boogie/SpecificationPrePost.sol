pragma solidity ^0.4.23;

contract SpecificationPrePost {
    /**
     * @notice precondition y >= 0
     * @notice postcondition result >= x
     */
    function test(int x, int y) public pure returns(int result) {
        return x + y;    
    }

    function violatePrecondition() public pure {
        test(2, -3);
    }
}