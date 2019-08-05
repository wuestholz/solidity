pragma solidity >=0.5.0;

library SafeMath {
    function add(uint a, uint b) internal pure returns (uint) {
        uint c = a + b;
        require(c >= a);
        return c;
    }
}

contract OverflowSideEffeect {
    using SafeMath for uint;

    function f() public pure {
        uint x = 1;
        uint y = 2;
        // No overflow, but the side effects of the condition
        // (the call to add) has to go before the overflow
        // conditions
        if (x + x.add(y) > 0) {

        }
    }
}