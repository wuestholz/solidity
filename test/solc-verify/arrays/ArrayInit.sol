pragma solidity ^0.5.0;

contract ArrayInit {

    struct S { int x; }

    int[2] a;
    int[] b;

    S[1] ss;

    int[2][3] ma;

    constructor() public {
        assert(a.length == 2);
        assert(a[0] == 0);
        assert(a[1] == 0);

        assert(b.length == 0);

        assert(ss.length == 1);
        assert(ss[0].x == 0);

        assert(ma[0][0] == 0);
        assert(ma[0].length == 2);
    }

    // This is here to be run by truffle
    function() external payable {
    }
}