pragma solidity >=0.5.0;

/**
 * @notice invariant address(this).balance == 0
 */
contract NonPayable {
    function() external payable {
        require(false);
    }
}

contract ToBeKilled {
    NonPayable public p;

    /// @notice postcondition p == np
    constructor(NonPayable np) public {
        p = np;
    }

    /// @notice postcondition address(p).balance >= msg.value
    function() external payable {
        selfdestruct(address(p));
    }
}

/// @notice invariant p == tbk.p()
contract SelfDestruct {

    NonPayable p;
    ToBeKilled tbk;

    constructor() public {
        p = new NonPayable();
        tbk = new ToBeKilled(p);
    }

    function() external payable {
        require(msg.value > 0);
        (bool b,) = address(tbk).call.value(msg.value)("");
        require(b);
        assert(address(p).balance > 0);
    }

}
