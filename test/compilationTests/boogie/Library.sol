pragma solidity ^0.4.23;

library MathLib {
    function add(uint256 a, uint256 b) internal pure returns (uint256) {
        return a + b;
    }
}

contract Library {
    using MathLib for uint256;

    function someFunc() public pure returns (uint256) {
        uint256 x = 5;
        return x.add(10); // Does not work yet
    }

    function otherFunc() public pure returns (uint256) {
        return MathLib.add(1, 2);
    }

    function __verifier_main() pure public {
        assert(someFunc() == 15);
        assert(otherFunc() == 3);
    }
}
