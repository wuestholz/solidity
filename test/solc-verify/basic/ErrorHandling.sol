pragma solidity >=0.5.0;

contract ErrorHandling {

    // Assertion should hold
    function abs_sound(int param) pure public returns (int) {
        int result = param;
        if (result < 0) result *= -1;
        assert(result >= 0);
        return result;
    }

    // Assertion should fail
    function abs_unsound(int param) pure public returns (int) {
        int result = param;
        if (result < 0) result += -1;
        assert(result >= 0);
        return result;
    }

    // Assertion should hold given the requirements
    function add(int x, int y) pure public returns (int) {
        require(x > 0);
        require(y > 0);
        int result = x + y;
        assert(result > x);
        assert(result > y);
        return result;
    }

    // Assertion should hold due to revert
    function add2(int x, int y) pure public returns (int) {
        if (x <= 0 || y <= 0) revert();
        int result = x + y;
        assert(result > x);
        assert(result > y);
        return result;
    }
}