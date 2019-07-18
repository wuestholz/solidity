pragma solidity >=0.5.0;
pragma experimental ABIEncoderV2;

contract StructsNew {
    struct S {
        int x;
    }

    function f(S memory s) public payable {
        S memory sm1 = S(1);
        S memory sm2 = S(2);
        S memory sm3 = sm1;

        s.x = 5;

        assert(sm1.x != sm2.x);
        assert(sm1.x == sm3.x);

        sm1.x = 3;

        assert(sm1.x != sm2.x);
        assert(sm1.x == sm3.x);
    }
}