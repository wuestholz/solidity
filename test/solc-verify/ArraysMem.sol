pragma solidity >=0.5.0;

contract ArraysMem {

    /// @notice postcondition x[0] == 5
    function returnArray() public pure returns (int[] memory x) {
        int[] memory a = new int[](1);
        a[0] = 5;
        return a;
    }

    function() external payable {
        int[] memory a1 = new int[](2);
        a1[0] = 1;
        a1[1] = 2;
        assert(a1[0] == 1);
        assert(a1[1] == 2);

        int[] memory a2 = a1; // Reference copy
        assert(a1[0] == 1);
        assert(a1[1] == 2);
        assert(a2[0] == 1);
        assert(a2[1] == 2);

        a2[0] = 3; // Changes original as well

        assert(a1[0] == 3);
        assert(a1[1] == 2);
        assert(a2[0] == 3);
        assert(a2[1] == 2);

        // Array returned by function
        assert(returnArray()[0] == 5);
    }
}