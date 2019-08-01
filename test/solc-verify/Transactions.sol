pragma solidity >=0.5.0;

contract Base {
    mapping(address=>int) public m;

    /// @notice postcondition m[msg.sender] == v
    function f(int v) public {
        m[msg.sender] = v;
    }
}

contract Transactions is Base {

    function() external payable {
        require(msg.sender != address(this));

        f(4); // Internal
        assert(m[msg.sender] == 4);

        Base.f(5); // Internal
        assert(m[msg.sender] == 5);

        this.f(6); // External
        assert(m[address(this)] == 6);
    }
}