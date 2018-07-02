pragma solidity ^0.4.23;

contract Arrays {
    uint[] arr;

    function index(uint i) view public returns (uint) {
        return arr[i];
    }
}