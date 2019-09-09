pragma solidity >=0.5.0;

contract Base {
    constructor() public {
        f();
    }

    function f() public pure {}
}

contract Derived is Base {

}
