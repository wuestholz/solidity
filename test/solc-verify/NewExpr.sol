pragma solidity >=0.5.0;

/** @notice invariant x >= 0 */
contract A {
    int private x;

    /** @notice postcondition x == 0 */
    constructor() public {}

    function incr() public { x++; }

    /** @notice postcondition r == x */
    function getX() public view returns (int r) { return x; }

    /** @notice postcondition x == _x */
    function setX(int _x) public { require(_x >= 0); x = _x; }
}

contract NewExpr {
    function single() public {
        A a = new A();
        a.incr();
        assert(a.getX() >= 0); // Should hold
    }

    function multiple() public {
        A a1 = new A();
        assert(a1.getX() == 0); // Should hold
        a1.setX(1);
        assert(a1.getX() == 1); // Should hold
        A a2 = new A();
        assert(a2.getX() == 0); // Should hold
        assert(a1.getX() == 1); // Should hold
    }

    function() external payable {
        single();
        multiple();
    }
}