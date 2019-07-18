pragma solidity >=0.5.0;

contract Units {
    function() external payable {
        assert(1 seconds == 1);
        assert(1 minutes == 60 seconds);
        assert(1 hours == 3600 seconds);
        assert(1 hours == 60 minutes);
        assert(1 days == 24 hours);
        assert(1 weeks == 7 days);

        assert(1 wei == 1);
        assert(1 szabo == 1e12);
        assert(1 finney == 1e15);
        assert(1 ether == 1e18);
    }
}