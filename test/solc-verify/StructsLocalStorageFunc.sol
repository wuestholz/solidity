pragma solidity >=0.5.0;

contract StructsLocalStorageFunc {
    struct S {
        int x;
    }

    struct U {
        S s1;
        S s2;
    }

    S s;
    U[] u;

    function set_x_with_ptr(S storage s_ptr, int _x) internal {
        s_ptr.x = _x;
    }

    function get_ptr(uint i, bool cond) internal view returns (S storage) {
        if (cond) return u[i].s1;
        else return u[i].s2;
    }

    function() external payable {
        // Calling a function
        set_x_with_ptr(s, 1);
        set_x_with_ptr(u[0].s1, 2);
        set_x_with_ptr(u[0].s2, 3);
        assert(s.x == 1);
        assert(u[0].s1.x == 2);
        assert(u[0].s2.x == 3);

        // Returning from function
        u[1].s1.x = 1;
        u[2].s2.x = 2;
        S storage s_ptr = get_ptr(1, true);
        assert(s_ptr.x == 1);
        s_ptr = get_ptr(2, false);
        assert(s_ptr.x == 2);
    }
}