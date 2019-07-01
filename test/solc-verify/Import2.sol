pragma solidity >=0.5.0;

import "Import1.sol";

contract B is A {
    function g() public pure {
        uint x = 1;
        x = f(1);
        assert(x == 1);
    }
}