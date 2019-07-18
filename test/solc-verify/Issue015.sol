pragma solidity >=0.5.0;

library L {}
contract C {
    function f() public {
        new L();
    }
}
