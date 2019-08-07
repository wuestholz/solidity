pragma solidity >=0.5.0;
pragma experimental ABIEncoderV2;

/// @notice invariant __verifier_sum_uint(items[__verifier_idx_uint]) == total
contract Simple {
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

/// @notice invariant __verifier_sum_uint(items[__verifier_idx_uint].price) == total
contract WithStruct {
    uint total;

    struct Item {
        uint price;
        uint id;
    }

    Item[] items;

    function insert(Item memory item) public {
        items.push(item);
        total += item.price;
    }

    function remove() public {
        total -= items[items.length-1].price;
        items.pop();
    }
}