pragma solidity >=0.5.0;
pragma experimental ABIEncoderV2;

contract CallData {
    struct S { int x; }

    S s1;

    function calldata_to_storage(S calldata s) external {
        // TODO: s1 = s; // Currently throws internal compiler error
        s1.x = s.x;
    }

    function calldata_to_memory(S calldata s) external pure {
        S memory sm = s;
        assert(sm.x == s.x);
    }
}