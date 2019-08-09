pragma solidity >=0.5.0;
pragma experimental ABIEncoderV2;

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

    function addToX(uint v) public {
        ss[msg.sender].x += v;
        xsum += v;
    }

    function addToZ(uint v) public {
        ss[msg.sender].t.z += v;
        zsum += v;
    }

    function setT(T memory t) public {
        uint oldz = ss[msg.sender].t.z;
        ss[msg.sender].t = t;
        zsum = zsum - oldz + t.z;
    }

    function setS(S memory s) public {
        uint oldz = ss[msg.sender].t.z;
        uint oldx = ss[msg.sender].x;
        ss[msg.sender] = s;
        zsum = zsum - oldz + s.t.z;
        xsum = xsum - oldx + s.x;
    }
}