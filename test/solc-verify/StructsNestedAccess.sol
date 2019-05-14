pragma solidity >=0.5.0;

contract StructsNestedAccess {
    struct S {
        int x;
        bool y;
        T t;
    }

    struct T {
        int z;
        mapping(address=>U) u;
    }

    struct U {
        int v;
    }

    mapping(address=>S) ss;

    function() external payable {
        ss[msg.sender].x = 1;
        ss[msg.sender].y = true;
        ss[msg.sender].t.z = 2;
        ss[msg.sender].t.u[address(this)].v = 3;

        assert(ss[msg.sender].x == 1);
        assert(ss[msg.sender].y == true);
        assert(ss[msg.sender].t.z == 2);
        assert(ss[msg.sender].t.u[address(this)].v == 3);
    }
}