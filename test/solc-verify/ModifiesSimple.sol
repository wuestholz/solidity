pragma solidity >=0.5.0;

contract ModifiesSimple {
    int x;
    int y;

    /**
     * @notice modifies *
     */
    function f() public {
        x = 0;
        y = 0;
    }

    /**
     * @notice modifies x if z > 0
     * @notice modifies x if z == 0
     */
    function g(int z) public {
        if (z >= 0)  x = z;
    }

}