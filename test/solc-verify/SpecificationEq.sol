pragma solidity >=0.5.0;
pragma experimental ABIEncoderV2;

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

/// @notice invariant __verifier_eq(s1, s2)
contract StructsEq {
    struct T {
        int z;
    }

    struct S {
        int x;
        T t;
    }

    S s1;
    S s2;

    function upd_correct(int v) public {
        s1.t.z = v;
        s2.t.z = v;
    }

    function upd_incorrect(int v) public {
        s1.t.z = v;
        s2.x = v;
    }

    function upd_from_mem_correct(S memory sm) public {
        s1 = sm;
        s2 = sm;
    }

    // If all members are equal, references do not matter
    function upd_from_mem_correct2(S memory sm1, S memory sm2) public {
        require(sm1.x == sm2.x);
        require(sm1.t.z == sm2.t.z);
        s1 = sm1;
        s2 = sm2;
    }

    function upd_from_mem_incorrect(S memory sm1, S memory sm2) public {
        s1 = sm1;
        s2 = sm2;
    }

    // Holds, same references
    /// @notice postcondition __verifier_eq(sm1.x, sm2.x)
    function mem_correct() public pure returns (S memory sm1, S memory sm2) {
        S memory sm = S(1, T(2));
        return (sm, sm);
    }

    // Does not hold, content is the same, but different references
    /// @notice postcondition __verifier_eq(sm1, sm2)
    function mem_incorrect() public pure returns (S memory sm1, S memory sm2) {
        sm1 = S(1, T(2));
        sm2 = S(1, T(2));
        return (sm1, sm2);
    }
}