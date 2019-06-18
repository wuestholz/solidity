pragma solidity >=0.5.0;

contract Continue {
    function whileloop() public pure {
        int x = 0;
        while (x < 10) {
            x++;
            if (x > 5) continue;
            x++;
            assert(x <= 6);
        }
    }

    function forloop() public pure {
        for (int i = 0; i < 10; i++) {
            if (i > 5) continue;
            assert(i <= 5);
        }
    }

    function nested() public pure {
        int x = 0;
        for (int i = 0; i < 10; i++) {
            if (i % 2 == 0) continue;
            while (x < i) {
                x++;
                if (x >= i) continue;
                assert(x < i);
                x++;
            }
            assert(i % 2 != 0);
        }
    }
}