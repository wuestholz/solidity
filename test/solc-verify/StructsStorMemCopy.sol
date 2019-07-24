pragma solidity >=0.5.0;

contract StructsStorMemCopy {
    struct S {
        int x;
        T t;
    }

    struct T {
        int z;
    }

    S st;

    function storToMem() public {
        // Initialize a storage and a memory struct
        S memory sm = S(1, T(2));
        st.x = 3;
        st.t.z = 4;

        assert(sm.x == 1);
        assert(sm.t.z == 2);
        assert(st.x == 3);
        assert(st.t.z == 4);

        // Deep copy from storage to memory
        sm = st;

        assert(sm.x == 3);
        assert(sm.t.z == 4);
        assert(st.x == 3);
        assert(st.t.z == 4);

        // Change storage, memory should not change
        st.x = 1;
        st.t.z = 2;

        assert(sm.x == 3);
        assert(sm.t.z == 4);
        assert(st.x == 1);
        assert(st.t.z == 2);
    }

    function memToStor() public {
        // Initialize a storage and a memory struct
        S memory sm = S(1, T(2));
        st.x = 3;
        st.t.z = 4;

        assert(sm.x == 1);
        assert(sm.t.z == 2);
        assert(st.x == 3);
        assert(st.t.z == 4);

        // Deep copy from memory to storage
        st = sm;

        assert(sm.x == 1);
        assert(sm.t.z == 2);
        assert(st.x == 1);
        assert(st.t.z == 2);

        // Change memory, storage should not change
        sm.x = 3;
        sm.t.z = 4;

        assert(sm.x == 3);
        assert(sm.t.z == 4);
        assert(st.x == 1);
        assert(st.t.z == 2);

        // Deep copy from storage to memory
        S memory sm2;
        sm2 = st;

        assert(sm2.x == 1);
        assert(sm2.t.z == 2);
        assert(st.x == 1);
        assert(st.t.z == 2);

        // Change storage, memory should not change
        st.x = 3;
        st.t.z = 4;

        assert(sm2.x == 1);
        assert(sm2.t.z == 2);
        assert(st.x == 3);
        assert(st.t.z == 4);
    }

    function returnMem() internal view returns (S memory s) {
        return st;
    }

    function checkReturnMem() public {
        st.x = 1;
        S memory sm = returnMem();
        st.x = 2;
        assert(sm.x == 1);
    }

    function() external payable {
        storToMem();
        memToStor();
        checkReturnMem();
    }

}