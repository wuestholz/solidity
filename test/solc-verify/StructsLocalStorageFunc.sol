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
    U u;

    function set_x_with_ptr(S storage s_ptr, int _x) internal {
        s_ptr.x = _x;
    }

    function get_ptr(bool cond) internal view returns (S storage) {
        if (cond) return u.s1;
        else return u.s2;
    }

    function() external payable {
        // Calling a function
        S storage sl1 = s;
        S storage sl2 = u.s1;
        S storage sl3 = u.s2;
        set_x_with_ptr(sl1, 1);
        set_x_with_ptr(sl2, 2);
        set_x_with_ptr(sl3, 3);
        assert(s.x == 1);
        assert(u.s1.x == 2);
        assert(u.s2.x == 3);

        // Returning from function
        u.s1.x = 1;
        u.s2.x = 2;
        S storage sl4 = get_ptr(true);
        assert(sl4.x == 1);
        sl4 = get_ptr(false);
        assert(sl4.x == 2);
    }
}