pragma solidity >=0.5.0;

contract Loops {

    function whileLoopFunc(uint param) private pure returns (uint) {
        uint i = 0;
        uint result = param;

        /**
         * @notice invariant result == param + i
         */
        while (i < 10) {
            result = result + 1;
            i = i + 1;
        }

        return result;
    }

    function doWhileLoopFunc(uint param) private pure returns (uint) {
        uint i = 0;
        uint result = param;

        /**
         * @notice invariant result == param + i
         */
        do {
            result = result + 1;
            i = i + 1;
        } while (i < 10);

        return result;
    }

    function forLoopFunc(uint param) private pure returns (uint) {
        uint result = param;

        /**
         * @notice invariant result == param + i
         */
        for (uint i = 0; i < 10; i = i + 1) {
            result = result + 1;
        }

        return result;
    }

    function breakLoop(uint param) private pure returns (uint) {
        uint i = 0;
        uint result = param;

        /**
         * @notice invariant result == param + i
         */
        while (i < 10) {
            result = result + 1;
            if (result >= 100) break;
            i = i + 1;
        }

        return result;
    }

    function whileSideEffect(uint param) private pure returns (uint) {
        uint i = 0;
        uint x;
        /**
         * @notice invariant x == i
         * @notice invariant i <= param
         */
        while((x = i) < param)
        {
            i++;
        }
        return x;
    }

    function forSideEffect(uint param) private pure returns (uint) {
        uint x;
        /**
         * @notice invariant x == i
         * @notice invariant i <= param
         */
        for (uint i = 0; (x = i) < param; i++) {}
        return x;
    }

    function() external payable {
        assert(whileLoopFunc(5) == 15);
        assert(doWhileLoopFunc(5) == 15);
        assert(forLoopFunc(9) == 19);
        assert(breakLoop(5) == 15);
        assert(breakLoop(95) == 100);
        assert(whileSideEffect(10) == 10);
        assert(forSideEffect(10) == 10);
    }

}