pragma solidity >=0.5.0;
pragma experimental ABIEncoderV2;

/// @notice invariant __verifier_sum_uint(ss[0].x) == xsum
/// @notice invariant __verifier_sum_uint(ss[0].t.z) == zsum
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

    mapping(uint=>S) ss;

    function addToX(uint i, uint v) public {
        ss[i].x += v;
        xsum += v;
    }

    function addToZ(uint i, uint v) public {
        ss[i].t.z += v;
        zsum += v;
    }

    function setT(uint i, T memory t) public {
        uint oldz = ss[i].t.z;
        ss[i].t = t;
        zsum = zsum - oldz + t.z;
    }

    function setS(uint i, S memory s) public {
        uint oldz = ss[i].t.z;
        uint oldx = ss[i].x;
        ss[i] = s;
        zsum = zsum - oldz + s.t.z;
        xsum = xsum - oldx + s.x;
    }
}