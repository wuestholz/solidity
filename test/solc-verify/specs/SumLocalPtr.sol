pragma solidity >=0.5.0;

/// @notice invariant __verifier_sum_uint(ss[__verifier_idx_address].x) == xsum
/// @notice invariant __verifier_sum_uint(ss[__verifier_idx_address].t.z) == zsum
contract C {

    uint xsum = 0;
    uint zsum = 0;

    struct T {
        uint z;
    }

    struct S {
        uint x;
        T t;
    }

    mapping(address=>S) ss;

    // Not used, but needed to test how the sum works if local pointers can point to multiple members
    S s1;
    T t1;

    function addToX(uint v) public {
        S storage s = ss[msg.sender]; // Try to trick the sum function with local pointer
        addToXinternal(s, v);
    }

    function addToXinternal(S storage s, uint v) internal {
        s.x += v;
        xsum += v;
    }

    function addToXincorrect(uint v) public {
        S storage s = ss[msg.sender]; // Try to trick the sum function with local pointer
        s.x += v;
        // Error: sum not updated
    }

    function addToXofS1(uint v) public {
        S storage s = s1;
        s.x += v;
        // OK, sum should not be updated here, s is pointing to s1
    }

    function addToZ(uint v) public {
         // Try to trick the sum function with local pointer
        addToZinternal(ss[msg.sender].t, v);
    }

    function addToZinternal(T storage t, uint v) internal {
        t.z += v;
        zsum += v;
    }
}