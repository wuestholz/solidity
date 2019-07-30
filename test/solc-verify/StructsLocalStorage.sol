pragma solidity >=0.5.0;

contract StructsLocalStorage {
    struct S {
        int x;
        T t;
    }

    struct T {
        int z;
    }

    T t1;
    S s1;
    S s2;

    function testSimple() public {
        s1.x = 1;
        s1.t.z = 2;
        assert(s1.x == 1);
        assert(s1.t.z == 2);

        S storage sl = s1;
        assert(sl.x == 1);
        assert(sl.t.z == 2);

        s1.x = 3;
        s1.t.z = 4;
        assert(sl.x == 3);
        assert(sl.t.z == 4);

        sl.x = 5;
        sl.t.z = 6;
        assert(s1.x == 5);
        assert(s1.t.z == 6);
    }

    function testMember() public {
        s1.x = 1;
        s1.t.z = 2;
        t1.z = 3;
        assert(s1.x == 1);
        assert(s1.t.z == 2);
        assert(t1.z == 3);

        T storage tl = s1.t;
        tl.z = 4;

        assert(s1.x == 1);
        assert(s1.t.z == 4);
        assert(t1.z == 3);

        tl = t1;
        tl.z = 5;

        assert(s1.x == 1);
        assert(s1.t.z == 4);
        assert(t1.z == 5);
    }

    function testConditional(bool b) public {
        // Set three members to 1
        t1.z = 1;
        s1.t.z = 1;
        s2.t.z = 1;
        // Conditionally point to one of the first two
        T storage ptr = t1;
        if (b) ptr = s1.t;
        ptr.z = 2; // Change

        // The third one should not change regardless condition
        assert(s2.t.z == 1);
    }

    S[2] s_arr;

    function testArray() public {
        s_arr[0].x = 1;
        s_arr[0].t.z = 2;
        s_arr[1].x = 3;
        s_arr[1].t.z = 4;

        uint i = 0;
        S storage sl = s_arr[i];
        i = i + 1;
        sl.x = 5;
        sl.t.z = 6;
        assert(s_arr[0].x == 5);
        assert(s_arr[0].t.z == 6);
        assert(s_arr[1].x == 3);
        assert(s_arr[1].t.z == 4);
    }

    function testArrayMember() public {
        s_arr[0].x = 1;
        s_arr[0].t.z = 2;
        s_arr[1].x = 3;
        s_arr[1].t.z = 4;

        uint i = 0;
        T storage tl = s_arr[i].t;
        i = i + 1;
        tl.z = 5;
        assert(s_arr[0].x == 1);
        assert(s_arr[0].t.z == 5);
        assert(s_arr[1].x == 3);
        assert(s_arr[1].t.z == 4);
    }

    T[2] t_arr;

    function testReassign() public {
        t_arr[0].z = 1;
        t_arr[1].z = 2;
        assert(t_arr[0].z == 1);
        assert(t_arr[1].z == 2);

        uint i = 0;
        T storage tl = t_arr[i];
        i++;
        tl.z = 3;
        assert(t_arr[0].z == 3);
        assert(t_arr[1].z == 2);

        tl = t_arr[i]; // Reassign
        assert(t_arr[0].z == 3);
        assert(t_arr[1].z == 2);

        tl.z = 4;
        assert(t_arr[0].z == 3);
        assert(t_arr[1].z == 4);
    }

    struct U {
        T[5] tt;
    }

    U[5] uu;

    function testAlternatingNesting() public {
        uu[1].tt[2].z = 12;
        uu[2].tt[1].z = 21;

        T storage tl = uu[1].tt[2];
        assert(tl.z == 12);
    }

    function() external payable {
        testSimple();
        testMember();
        testConditional(true);
        testConditional(false);
        testArray();
        testArrayMember();
        testReassign();
        testAlternatingNesting();
    }
}