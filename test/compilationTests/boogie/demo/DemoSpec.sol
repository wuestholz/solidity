pragma solidity ^0.4.23;

/** @notice invariant x == y */
contract DemoSpec {
    uint x = 0;
    uint y = 0;

    function addOne() public {
        x++;
        y++;
    }

    function add(uint amount) public {
        /** @notice invariant x == y */
        for (uint i = 0; i < amount; i++) {
            addOne();
        }
    }
}