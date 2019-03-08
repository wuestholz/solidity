pragma solidity ^0.4.24;

contract ModTccs {
    function f(int16 x) public pure {
        assert(x + 0 == x + 0 + 0);
    }

    function g(uint32 x, uint32 y) public pure {
        require(x < 10 && y < 10); // No overflow
        uint32 z;
        z = x + y;
        assert(z >= x);
        assert(z >= y);
    }

    function h(uint32[] arr) public {
        require(arr[0] < 10 && arr[1] < 10);
        uint32 z;
        z = arr[0] + arr[1];
        assert(z >= arr[0]);
        assert(z >= arr[1]);
    }
}
