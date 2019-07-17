pragma solidity >=0.5.0;

contract ArraysMemWarning {
    function f(uint x) public pure {
        require(x > 0);
        int[] memory a1 = new int[](x);
        assert(a1.length == x);
        assert(a1[0] == 0); // Fails because length is not known compile time
    }
}