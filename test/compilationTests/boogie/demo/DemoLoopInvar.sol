pragma solidity ^0.4.23;

contract DemoLoopInvar {
    function test() public pure {
        int i = 0;
        int x = 0;
        /** @notice invariant i == x */
        while (i < 10) {
            i++;
            x++;
        }
        assert(x == i);
    }
}