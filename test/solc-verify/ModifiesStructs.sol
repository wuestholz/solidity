pragma solidity >=0.5.0;

contract ModifiesStructs {

    struct T {
        int z;
    }

    struct S {
        int x;
        T t;
    }

    S s;
    mapping(address=>S) ss;
    int x;

    /** @notice modifies x */
    function modifyStructIncorrect(int value) public {
        s.t.z = value;
        x = value;
    }

    /** @notice modifies s */
    function modifyStructCorrect(int value) public {
        s.t.z = value;
    }

    /** @notice modifies s[this] if value > 0 */
    function modifyStructMapIncorrect(int value) public {
        ss[msg.sender].t.z = value;
    }

    /** @notice modifies ss[address(this)] if value > 0 */
    function modifyStructMapCorrect(int value) public {
        require(value > 0);
        ss[address(this)].t.z = value;
    }
}