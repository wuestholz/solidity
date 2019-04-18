pragma solidity >=0.5.0;

contract FixedBytes {
    bytes32 arr;

    // Safe, we check that index is in bounds
    function getValueSafe(uint i) public view returns (byte) {
        if (i < arr.length) return arr[i];
        return 0;
    }

    // Safe, we check that index is in bounds
    function getValueUnsafe(uint i) public view returns (byte) {
        return arr[i];
    }

    // Unsafe we don't know i
    function singleByte(bytes1 param, uint i) public pure returns (byte) {
        assert(param.length == 1);
        return param[i];
    }

}