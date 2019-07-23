pragma solidity >=0.5.0;

contract Swap {
    function() external payable {
        uint x = 1;
        uint y = 2;
        (x, y) = (y, x);
        assert(x == 2);
        assert(y == 1);
    }
}