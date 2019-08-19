pragma solidity >=0.5.0;

contract C {
    function f1() public pure { // OK
        uint i = 0;
        uint x = 0;
        /// @notice invariant x == i
        do {
            i++;
            x = i;
        } while(i < 10);
        assert(x == 10);
    }

    function f2() public pure { // Does not hold before first iteration
        uint i = 0;
        uint x = 1;
        /// @notice invariant x == i
        do {
            i++;
            x = i;
        } while(i < 10);
    }

    function f3() public pure { // Does not hold after first iteration
        uint i = 0;
        uint x = 0;
        /// @notice invariant x == i
        do {
            x = i;
            i++;
        } while(i < 10);
    }

    function f4() public pure { // Holds after first iteration, but not after further iterations
        uint i = 0;
        uint x = 0;
        /// @notice invariant x == i
        do {
            i++;
            x = 1;
        } while(i < 10);
    }
}