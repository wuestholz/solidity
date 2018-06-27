pragma solidity ^0.4.17;

contract SimpleArithmetic {
    function and(bool a, bool b) pure public returns (bool) {
        return a && b;
    }
    function or(bool a, bool b) pure public returns (bool) {
        return a || b;
    }
    function not(bool a) pure public returns (bool) {
        return !a;
    }

    function add(uint a, uint b) pure public returns (uint) {
        return a + b;
    }
    function sub(uint a, uint b) pure public returns (uint) {
        return a - b;
    }
    function mul(uint a, uint b) pure public returns (uint) {
        return a * b;
    }
    function div(uint a, uint b) pure public returns (uint) {
        return a / b;
    }
    function mod(uint a, uint b) pure public returns (uint) {
        return a % b;
    }
    function exp(uint a, uint b) pure public returns (uint) {
        return a ** b;
    }
    function minus(int a) pure public returns (int) {
        return -a;
    }
    function doSomethingComplex(uint a, uint b, uint c) pure public returns (uint){
        return ((a + b) * 1234 / c) % 56;
    }

    function assignAdd(uint a, uint b) pure public returns (uint) {
        uint result = a;
        result += b;
        return result;
    }
    function assignSub(uint a, uint b) pure public returns (uint) {
        uint result = a;
        result -= b;
        return result;
    }
    function assignMul(uint a, uint b) pure public returns (uint) {
        uint result = a;
        result *= b;
        return result;
    }
    function assignDiv(uint a, uint b) pure public returns (uint) {
        uint result = a;
        result /= b;
        return result;
    }
    function assignMod(uint a, uint b) pure public returns (uint) {
        uint result = a;
        result %= b;
        return result;
    }

    function eq(uint a, uint b) pure public returns (bool) {
        return a == b;
    }
    function neq(uint a, uint b) pure public returns (bool) {
        return a != b;
    }
    function lt(uint a, uint b) pure public returns (bool) {
        return a < b;
    }
    function gt(uint a, uint b) pure public returns (bool) {
        return a > b;
    }
    function lte(uint a, uint b) pure public returns (bool) {
        return a <= b;
    }
    function gte(uint a, uint b) pure public returns (bool) {
        return a >= b;
    }
}