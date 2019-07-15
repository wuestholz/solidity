pragma solidity >=0.5.0;

contract Arrays {
    uint[] arr;

    function readStateArr(uint i) view public returns (uint) {
        if (i < arr.length) return arr[i];
        return 0;
    }

    function writeStateArr(uint i, uint value) public {
        arr[i] = value;
    }

    // TODO: assigning to parameters does not work in Boogie
    //function writeParamArr(uint[] paramArr, uint i, uint value) pure public returns (uint) {
    //    paramArr[i] = value;
    //    return paramArr[i];
    //}

    function readParamArr(uint[] memory paramArr, uint i) pure public returns (uint) {
        if (i < paramArr.length) return paramArr[i];
        return 0;
    }

    function callWithLocalArray(uint[] memory paramArr) pure public returns (uint) {
        return readParamArr(paramArr, 123);
    }

    function callWithStateArray() view public returns (uint) {
        return readParamArr(arr, 456);
    }
}