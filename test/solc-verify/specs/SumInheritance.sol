pragma solidity >=0.5.0;

/// @notice invariant __verifier_sum_int(map) == total
contract Base {
    mapping(address=>int) map;
    int total;

    function add(int x) public {
        map[msg.sender] += x;
        total += x;
    }
}

/// @notice invariant __verifier_sum_int(map) == total
contract Derived is Base {
    function sub(int x) public {
        map[msg.sender] -= x;
        total -= x;
    }
}