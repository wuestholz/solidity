pragma solidity ^0.4.23;

contract Arrays {
    uint[] arr;

    function index(uint i) view public returns (uint) {
        if (i < arr.length) return arr[i];
        return 0;
    }

    function paramArray(uint[] paramArr, uint i) pure public returns (uint) {
        if (i < paramArr.length) return paramArr[i];
        return 0;
    }

    function callWithLocalArray(uint[] paramArr) pure public returns (uint) {
        return paramArray(paramArr, 123);
    }

    function callWithStateArray() view public returns (uint) {
        return paramArray(arr, 456);
    }
}