pragma solidity ^0.4.23;

contract IfElse {
    function ifthenelse(uint param) private pure returns (uint) {
        if (param < 10) {
            return 10;
        } else if (param < 20) {
            return 20;
        } else {
            return 30;
        }
    }

    function onlyif(uint param) private pure returns (uint) {
        if (param < 10) {
            return 10;
        }
        return 20;
    }

    function conditional(uint param) private pure returns (uint) {
        return param > 10 ? 10 : 0;
    }

    function __verifier_main() public pure {
        assert(ifthenelse(5) == 10);
        assert(ifthenelse(15) == 20);
        assert(ifthenelse(25) == 30);
        assert(onlyif(5) == 10);
        assert(onlyif(15) == 20);
        assert(conditional(5) == 0);
        assert(conditional(50) == 10);
    }
}