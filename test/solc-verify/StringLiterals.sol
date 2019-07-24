pragma solidity >=0.5.0;

contract StringLiterals {

    string s1 = "ABC";
    string s2 = "  ABC  ";
    string s3 = "ABC";

    mapping(string=>int) m;

    constructor() public {
        m[s1] = 1;
        m[s2] = 2;
        assert(m[s3] == 1); // s1 is the same as s3
    }

    // Just for truffle tests
    function() external payable {
    }
}