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
    NonPayable p;
    constructor(NonPayable np) public {
        p = np;
    }
    function() external payable {
        selfdestruct(address(p));
    }
}

contract SelfDestruct {

    NonPayable p;
    ToBeKilled tbk;

    constructor() public {
        p = new NonPayable();
        tbk = new ToBeKilled(p);
    }

    function() external payable {
        (bool b,) = address(tbk).call.value(msg.value)("");
        assert(b);
        assert(address(p).balance > 0);
    }

}
