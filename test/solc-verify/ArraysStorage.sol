pragma solidity >=0.5.0;

contract ArraysStorage {
    int[2] x;
    int[2] y;

    function() external payable {
        x[0] = 1;
        x[1] = 2;
        y[0] = 3;
        y[1] = 4;

        assert(x[0] == 1);
        assert(x[1] == 2);
        assert(y[0] == 3);
        assert(y[1] == 4);

        y = x; // Deep copy

        assert(x[0] == 1);
        assert(x[1] == 2);
        assert(y[0] == 1);
        assert(y[1] == 2);

        x[0] = 3; // Will not change y
        x[1] = 4;

        assert(x[0] == 3);
        assert(x[1] == 4);
        assert(y[0] == 1);
        assert(y[1] == 2);
    }
}