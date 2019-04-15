pragma solidity >=0.5.0;

contract FixedBytes {
    bytes32 arr;

    function getValue(uint i) public view returns (byte) {
        if (0 <= i && i < arr.length) return arr[i];
        return 0;
    }

    function singleByte(bytes1 param, uint i) public pure returns (byte) {
        assert(param.length == 1);
        return param[i];
    }

}