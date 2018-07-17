pragma solidity ^0.4.23;

contract Assignments {
    uint contractVar;

    function init() public {
        contractVar = 10;
    }

    function doSomething(uint param) public returns (uint) {
        uint localVar = 5;
        localVar += param;
        contractVar += localVar;
        return contractVar;
    }

    function chained(uint param) public returns (uint) {
        uint localVar;
        localVar = contractVar = param + 1;
        return localVar;
    }

    function chained2(uint param) public returns (uint) {
        uint localVar = param;
        contractVar = localVar += 1;
        return contractVar;
    }

    function incrDecr(uint param) public pure returns (uint) {
        uint localVar = param;
        uint x = localVar++;
        assert(x == localVar - 1); // x should have the old value
        uint y = ++localVar;
        assert(y == localVar); // y should have the new value
        uint z = localVar--;
        assert(z == localVar + 1); // z should have the old value
        uint t = --localVar;
        assert(t == localVar); // t should have the new value
        return localVar;
    }

    function incrContractVar() public returns (uint) {
        uint x = contractVar++;
        assert(x == contractVar - 1);
        uint y = ++contractVar;
        assert(y == contractVar);
        return contractVar;
    }

    function complexStuff(uint param) pure public returns (uint) {
        uint x = param;
        return (++x) + (x++);
    }

    function __verifier_main() public {
        init();
        assert(doSomething(20) == 35);
        assert(doSomething(10) == 50);
        assert(chained(5) == 6);
        assert(chained2(5) == 6);
        assert(incrDecr(5) == 5);
        init();
        assert(incrContractVar() == 12);
        assert(complexStuff(0) == 2);
    }
}