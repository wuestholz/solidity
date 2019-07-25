pragma solidity >=0.5.0;

contract StructsLocalStorage {
    struct S {
        int x;
        T t;
    }

    struct T {
        int z;
    }

    S ss;

    function testSimple() public {
        ss.x = 1;
        ss.t.z = 2;
        assert(ss.x == 1);
        assert(ss.t.z == 2);

        S storage sl = ss;

        sl.x = 3;
        sl.t.z = 4;
        assert(ss.x == 3);
        assert(ss.t.z == 4);
    }

    function testMember() public {
        ss.x = 1;
        ss.t.z = 2;
        assert(ss.x == 1);
        assert(ss.t.z == 2);

        T storage tl = ss.t;
        tl.z = 3;

        assert(ss.x == 1);
        assert(ss.t.z == 3);
    }

    mapping(uint=>S) s_map;

    function testMapping() public {
        s_map[0].x = 1;
        s_map[0].t.z = 2;
        s_map[1].x = 3;
        s_map[1].t.z = 4;

        uint i = 0;
        S storage sl = s_map[i];
        i = i + 1;
        sl.x = 5;
        sl.t.z = 6;
        assert(s_map[0].x == 5);
        assert(s_map[0].t.z == 6);
        assert(s_map[1].x == 3);
        assert(s_map[1].t.z == 4);
    }

    function testMappingMember() public {
        s_map[0].x = 1;
        s_map[0].t.z = 2;
        s_map[1].x = 3;
        s_map[1].t.z = 4;

        uint i = 0;
        T storage tl = s_map[i].t;
        i = i + 1;
        tl.z = 5;
        assert(s_map[0].x == 1);
        assert(s_map[0].t.z == 5);
        assert(s_map[1].x == 3);
        assert(s_map[1].t.z == 4);
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

    function() external payable {
        testSimple();
        testMember();
        testMapping();
        testMappingMember();
        testArray();
        testArrayMember();
    }
}