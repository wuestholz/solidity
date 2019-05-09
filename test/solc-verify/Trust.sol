pragma solidity >=0.5.0;

/** @notice invariant x >= 0 */
contract A {
    int private x;

    function incr() public { x++; }

    /** @notice postcondition r == x */
    function getX() public view returns (int r) { return x; }
}

contract B {
    // This reference can be trusted as it is only assigned with 'new' internally
    A a;

    constructor() public {
        a = new A(); // Calling the constsructor will establish invariant
        a.incr();
        assert(a.getX() >= 0); // Should hold
    }

    function f() public {
        a.incr(); // Invariant needs to be assumed (trusted reference)
        assert(a.getX() >= 0); // Should hold
    }
}

contract C {
    A a; // Cannot (always) be trusted

    constructor() public {
        a = new A(); // Calling the constsructor will establish invariant
        assert(a.getX() >= 0); // Should hold
    }

    function f(A _a) public view {
        assert(_a.getX() >= 0); // Should fail, annotations should not be considered
    }

    function g(A _a) public {
        a = _a;
        assert(a.getX() >= 0); // Should fail, annotations should not be considered
    }

    function h() public view {
        assert(a.getX() >= 0); // Should fail, annotations should not be considered
    }
}