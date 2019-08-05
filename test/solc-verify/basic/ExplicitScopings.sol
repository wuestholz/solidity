pragma solidity >=0.5.0;

contract A {
    /** @notice postcondition x + 1 == y */
    function f(int x) public pure returns(int y) { return x + 1;}

    /** @notice postcondition x + 1 == y */
    function g(int x) public pure returns (int y) {
        return A.f(x); // Explicit scoping with current contract name
    }
}

contract B is A {
    function g(int x) public pure returns (int) {
        int z = A.g(x); // Explicit scoping with base name
        assert(z == x + 1);
        return z;
    }
    function h(int x) public pure returns (int) {
        int z = super.g(x); // Explicit scoping with super
        assert(z == x + 1);
        return z;
    }
}