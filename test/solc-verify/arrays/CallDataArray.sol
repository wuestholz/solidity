pragma solidity >=0.5.0;

contract CallData {
    uint[] stor;

    function calldata_to_memory(uint[] calldata arr) external pure {
        require(arr.length > 0);
        uint[] memory arr2 = arr;
        assert(arr2[0] == arr[0]);
    }

    function calldata_to_storage(uint[] calldata arr) external {
        require(arr.length > 0);
        stor = arr;
        assert(stor[0] == arr[0]);
        stor[0]++;
        assert(stor[0] != arr[0]);
    }
}