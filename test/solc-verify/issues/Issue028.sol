pragma solidity >=0.5.0;

contract Issue028 {
    function f() pure public {
        ((2**270) / 2**100, 1);
    }
}
