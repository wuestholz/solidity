pragma solidity >=0.5.0;

contract Layout {


    struct T {
        int z;
    }

    mapping(address=>T) ts;

    function() external payable {
        require(msg.sender != address(this));
        ts[msg.sender] = T(1);
        ts[address(this)] = ts[msg.sender];

        assert(ts[address(this)].z == 1);
        ts[msg.sender].z = 2;
        assert(ts[address(this)].z == 1);
    }
}