pragma solidity >=0.5.0;

/// @notice invariant __verifier_sum_uint(items[__verifier_idx_uint]) == total
contract C {
    uint total;
    uint[] items;

    function add(uint i, uint v) public {
        total += v;
        items[i] += v;
    }

    function insert(uint v) public {
        items.push(v);
        total += v;
    }

    function remove() public {
        total -= items[items.length-1];
        items.pop();
    }
}