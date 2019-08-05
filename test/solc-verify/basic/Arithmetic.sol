pragma solidity >=0.5.0;

contract Arithmetic {
    function and(bool a, bool b) private pure returns (bool) {
        return a && b;
    }
    function or(bool a, bool b) private pure returns (bool) {
        return a || b;
    }
    function not(bool a) private pure returns (bool) {
        return !a;
    }

    function add(uint a, uint b) private pure returns (uint) {
        return a + b;
    }
    function sub(uint a, uint b) private pure returns (uint) {
        return a - b;
    }
    function mul(uint a, uint b) private pure returns (uint) {
        return a * b;
    }
    function div(uint a, uint b) private pure returns (uint) {
        return a / b;
    }
    function mod(uint a, uint b) private pure returns (uint) {
        return a % b;
    }
    function minus(int a) private pure returns (int) {
        return -a;
    }
    function doSomethingComplex(uint a, uint b, uint c) private pure returns (uint){
        return ((a + b) * 1234 / c) % 56;
    }

    function assignAdd(uint a, uint b) private pure returns (uint) {
        uint result = a;
        result += b;
        return result;
    }
    function assignSub(uint a, uint b) private pure returns (uint) {
        uint result = a;
        result -= b;
        return result;
    }
    function assignMul(uint a, uint b) private pure returns (uint) {
        uint result = a;
        result *= b;
        return result;
    }
    function assignDiv(uint a, uint b) private pure returns (uint) {
        uint result = a;
        result /= b;
        return result;
    }
    function assignMod(uint a, uint b) private pure returns (uint) {
        uint result = a;
        result %= b;
        return result;
    }

    function eq(uint a, uint b) private pure returns (bool) {
        return a == b;
    }
    function neq(uint a, uint b) private pure returns (bool) {
        return a != b;
    }
    function lt(uint a, uint b) private pure returns (bool) {
        return a < b;
    }
    function gt(uint a, uint b) private pure returns (bool) {
        return a > b;
    }
    function lte(uint a, uint b) private pure returns (bool) {
        return a <= b;
    }
    function gte(uint a, uint b) private pure returns (bool) {
        return a >= b;
    }

    function() external payable {
        assert(and(true, true) == true);
        assert(and(true, false) == false);
        assert(and(false, true) == false);
        assert(and(false, false) == false);

        assert(or(true, true) == true);
        assert(or(true, false) == true);
        assert(or(false, true) == true);
        assert(or(false, false) == false);

        assert(not(true) == false);
        assert(not(false) == true);

        assert(add(1, 2) == 3);
        assert(sub(5, 2) == 3);
        assert(mul(5, 2) == 10);
        assert(div(7, 2) == 3);
        assert(mod(10, 3) == 1);
        assert(minus(5) == -5);
        assert(minus(-3) == 3);
        assert(assignAdd(1, 2) == 3);
        assert(assignSub(5, 2) == 3);
        assert(assignMul(5, 2) == 10);
        assert(assignDiv(7, 2) == 3);
        assert(assignMod(10, 3) == 1);

        assert(eq(1, 1) == true);
        assert(eq(1, 2) == false);
        assert(neq(1, 1) == false);
        assert(neq(1, 2) == true);
        assert(lt(1, 1) == false);
        assert(lt(1, 2) == true);
        assert(lt(2, 1) == false);
        assert(gt(1, 1) == false);
        assert(gt(1, 2) == false);
        assert(gt(2, 1) == true);
        assert(lte(1, 1) == true);
        assert(lte(1, 2) == true);
        assert(lte(2, 1) == false);
        assert(gte(1, 1) == true);
        assert(gte(1, 2) == false);
        assert(gte(2, 1) == true);
    }
}