pragma solidity >=0.5.0;


/*
                          RHS

                Storage   Memory   Local st
               ----------------------------
      Storage | Deep      Deep     Deep
              |
LHS    Memory | Deep      Ref      Deep
              |
     Local st | Ref       Error    Ref
*/
contract Copying {
    struct S {
        int x;
    }

    S s1;
    S s2;

    function storToStor() public {
        s1.x = 1;
        s2.x = 2;

        s1 = s2; // Deep copy
        assert(s1.x == 2);
        s2.x = 3;
        assert(s1.x == 2);
    }

    function storToMem() public {
        S memory sm = S(3);
        s1.x = 1;
        sm = s1; // Deep copy
        assert(s1.x == 1);
        assert(sm.x == 1);
        s1.x = 2;
        assert(s1.x == 2);
        assert(sm.x == 1);
    }

    function storToLoc() public {
        s1.x = 1;
        S storage loc = s1; // Reference copy
        assert(loc.x == s1.x);
        s1.x++;
        assert(loc.x == s1.x);
        loc.x++;
        assert(loc.x == s1.x);
    }

    function memToMem() public pure {
        S memory sm1 = S(1);
        S memory sm2 = S(2);

        sm1 = sm2; // Reference copy
        assert(sm1.x == sm2.x);
        sm1.x++;
        assert(sm1.x == sm2.x);
    }

    function memToStor() public {
        S memory sm = S(3);
        s1 = sm; // Deep copy
        assert(s1.x == 3);
        assert(sm.x == 3);
        s1.x = 1;
        assert(s1.x == 1);
        assert(sm.x == 3);
    }

    function memToLoc() public {
        S memory sm = S(3);
        s1.x = 1;
        S storage sl = s1; // Local storage has to point to some storage
        assert(sl.x == 1);
        // sl = sm; // Memory cannot be directly assigned to local storage
        s1 = sm; // But assigning it to the storage where the local is pointing updates the local as well
        assert(sl.x == 3);
    }

    function locToLoc() public {
        s1.x = 1;
        s2.x = 2;
        S storage sl1 = s1;
        S storage sl2 = s2;
        assert(sl1.x == 1);
        assert(sl2.x == 2);

        sl1 = sl2; // Reference copy
        sl1.x = 3;
        assert(sl1.x == 3);
        assert(sl2.x == 3);
        assert(s1.x == 1);
        assert(s2.x == 3);
    }

    function locToStor() public {
        s1.x = 1;
        s2.x = 2;
        S storage sl1 = s1;
        s2 = sl1; // Deep copy
        assert(s2.x == 1);
        sl1.x++;
        assert(s2.x == 1);
    }

    function locToMem() public {
        s1.x = 1;
        S storage sl1 = s1;
        S memory sm = S(3);
        sm = s1; // Deep copy
        assert(sl1.x == 1);
        assert(sm.x == 1);
        sl1.x = 2;
        assert(sl1.x == 2);
        assert(sm.x == 1);
    }

    function() external payable {
        storToStor();
        storToMem();
        storToLoc();
        memToMem();
        memToStor();
        memToLoc();
        locToLoc();
        locToStor();
        locToMem();
    }

}