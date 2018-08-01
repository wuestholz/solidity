pragma solidity ^0.4.23;

contract SpecificationLoopInvar {
    function correct() public pure {
        int i = 0;
        int x = 0;
        /** @notice invariant i == x */
        while (i < 10) {
            i++;
            x++;
        }
        assert(x == i);
    }
    function cannotProveWithoutInvariant() public pure {
        int i = 0;
        int x = 0;
        while (i < 10) {
            i++;
            x++;
        }
        assert(x == i); // Cannot prove assertion without invariant
    }
    function doesNotHoldOnEntry() public pure {
        int i = 0;
        int x = 1; // Invariant does not hold on entry
        /** @notice invariant i == x */
        while (i < 10) {
            i++;
            x++;
        }
        assert(x == i);
    }
    function isNotMaintainedByLoop() public pure {
        int i = 0;
        int x = 0;
        /** @notice invariant i == x */
        while (i < 10) {
            i++; // Invariant not maintained by loop
        }
        assert(x == i);
    }
}