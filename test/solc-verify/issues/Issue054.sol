pragma solidity >=0.5.0;

contract C {
    struct S { bool f; }
    S s;
    function i() internal view returns (S storage c) {
        if ((c = s).f) {
        }
    }
}