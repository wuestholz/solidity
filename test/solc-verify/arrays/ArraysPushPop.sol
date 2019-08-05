pragma solidity >=0.5.0;

contract ArraysPushPop {

    int[] x;

    /// @notice modifies x[x.length-1]
    /// @notice modifies x.length
    /// @notice postcondition x.length == __verifier_old_uint(x.length) + 1
    /// @notice postcondition x[x.length-1] == n
    function insert(int n) public {
        x.push(n);
    }

    /// @notice modifies x
    function() external payable {
        require(x.length == 0);
        x.push(4);
        assert(x.length == 1);
        assert(x[0] == 4);
        insert(5);
        assert(x.length == 2);
        assert(x[0] == 4);
        assert(x[1] == 5);
        x.pop();
        assert(x.length == 1);
        assert(x[0] == 4);
        x.pop();
        assert(x.length == 0);
    }

}