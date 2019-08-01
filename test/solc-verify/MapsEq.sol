pragma solidity >=0.5.0;

/// @notice invariant __verifier_eq(x, y)
contract MapsEq {
    mapping(address=>uint) x;
    mapping(address=>uint) y;

    function upd_correct(uint v) public {
        x[msg.sender] = v;
        y[msg.sender] = v;
    }

    function upd_incorrect(uint v) public {
        x[msg.sender] = v;
        y[address(this)] = v;
    }

    // We need an explicit precondition for the constructor
    // to silence false alarms related to uninitialized maps
    // in Boogie
    
    /// @notice precondition __verifier_eq(x, y)
    constructor() public {}
}