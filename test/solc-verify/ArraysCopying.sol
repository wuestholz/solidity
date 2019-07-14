pragma solidity >=0.5.0;

contract ArraysCopying {
    int[] x;

    function() external payable {
        int[] memory m = new int[](2);
        m[0] = 1;
        m[1] = 2;

        x = m; // Copy

        assert(x[0] == 1);
        assert(x[1] == 2);

        x[0] = 3; // Does not change other

        assert(x[0] == 3);
        assert(x[1] == 2);
        assert(m[0] == 1);
        assert(m[1] == 2);

        m = x; // Copy

        assert(m[0] == 3);
        assert(m[1] == 2);

        m[0] = 4; // Does not change other

        assert(x[0] == 3);
        assert(x[1] == 2);
        assert(m[0] == 4);
        assert(m[1] == 2);
    }
}