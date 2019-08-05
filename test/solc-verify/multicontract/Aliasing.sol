pragma solidity >=0.5.0;

contract A {
    int x;

    /** @notice postcondition x == 0 */
    function reset() public { x = 0; }

    /** @notice postcondition r == x */
    function get() public view returns (int r) { return x; }
}

contract Aliasing is A {

    A a1;
    A a2;

    constructor() public {
        a1 = new A();
        a2 = new A();
    }

    function() external payable {
        require(a1 != a2);
        a1.reset();
        assert(a1.get() == 0); // Should hold
        a2.reset();
        assert(a2.get() == 0); // Should hold
        reset();
        assert(get() == 0); // Should hold
        assert(a1.get() == 0); // Should hold
        assert(a2.get() == 0); // Should hold
    }
}