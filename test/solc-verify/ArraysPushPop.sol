pragma solidity >=0.5.0;

contract ArraysPushPop {

    int[] x;

    function() external payable {
        require(x.length == 0);
        x.push(4);
        assert(x.length == 1);
        assert(x[0] == 4);
        x.push(5);
        assert(x.length == 2);
        assert(x[0] == 4);
        assert(x[1] == 5);
        x.pop();
        assert(x.length == 1);
        assert(x[0] == 4);
        x.pop();
        assert(x.length == 0);
    }

}