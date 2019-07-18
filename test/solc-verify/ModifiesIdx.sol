pragma solidity >=0.5.0;

contract ModifiesIdx {
    mapping(address=>int) xs;

    /**
     * @notice modifies xs[address(this)]
     * @notice modifies xs[msg.sender]
     */
    function f() public {
        xs[address(this)]++;
        xs[msg.sender]--;
    }

    /**
     * @notice modifies xs[address(this)] if x > 0
     * @notice modifies xs if x == 0
     * @notice modifies xs[msg.sender]
     */
    function g(int x) public {
        require(address(this) != msg.sender);
        if (x > 0) xs[address(this)]++;
        xs[msg.sender]--;
        if (x == 0) {
            xs[address(123)] = 5;
        }
    }

    /**
     * @notice modifies xs[address(this)] if x > 0
     * @notice modifies xs if x == 0
     * @notice modifies xs[msg.sender]
     */
    function g_incorrect(int x) public {
        require(address(this) != msg.sender);
        xs[address(this)]++;
        xs[msg.sender]--;
        if (x == 0) {
            xs[address(123)] = 5;
        }
    }
}