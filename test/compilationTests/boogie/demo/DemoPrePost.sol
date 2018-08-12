pragma solidity ^0.4.23;

contract DemoPrePost {
    /**
     * @notice precondition y >= 0
     * @notice postcondition result >= x
     */
    function test(int x, int y) public pure returns(int result) {
        return x + y;    
    }
}