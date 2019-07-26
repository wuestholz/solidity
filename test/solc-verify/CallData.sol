pragma solidity >=0.5.0;
pragma experimental ABIEncoderV2;

contract CallData {
    uint[] stor;

    function arr_calldata_to_memory(uint[] calldata arr) external pure {
        require(arr.length > 0);
        uint[] memory arr2 = arr;
        assert(arr2[0] == arr[0]);
    }

    function arr_calldata_to_storage(uint[] calldata arr) external {
        require(arr.length > 0);
        stor = arr;
        assert(stor[0] == arr[0]);
        stor[0]++;
        assert(stor[0] != arr[0]);
    }

    struct S { int x; }

    S s1;

    function struct_calldata_to_storage(S calldata s) external {
        // TODO: s1 = s; // Currently throws internal compiler error
        s1.x = s.x;
    }

    function struct_calldata_to_memory(S calldata s) external pure {
        S memory sm = s;
        assert(sm.x == s.x);
    }
}