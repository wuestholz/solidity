pragma solidity ^0.4.23;

library MathLib {
    function add(uint256 a, uint256 b) internal pure returns (uint256) {
        return a + b;
    }
}

contract C {
    using MathLib for uint256;

    function someFunc() public pure returns (uint256) {
        uint256 x = 5;
        //return x.add(10); // Does not work yet
        return x;
    }
}

contract D {
    function otherFunc() public pure returns (uint256) {
        return MathLib.add(1, 2);
    }
}