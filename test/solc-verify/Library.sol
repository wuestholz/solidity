pragma solidity >=0.5.0;

library MathLib {
    function add(uint256 a, uint256 b) internal pure returns (uint256) {
        return a + b;
    }
}

contract Library {
    using MathLib for uint256;

    function someFunc() private pure returns (uint256) {
        uint256 x = 5;
        return x.add(10);
    }

    function otherFunc() private pure returns (uint256) {
        return MathLib.add(1, 2);
    }

    function __verifier_main() public pure {
        assert(someFunc() == 15);
        assert(otherFunc() == 3);
    }
}
