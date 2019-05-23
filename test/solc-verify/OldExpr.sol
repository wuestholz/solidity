pragma solidity >=0.5.0;

contract OldExpr {
    address owner;
    int8 counter;

    constructor() public {
        owner = msg.sender;
    }

    // Correct version, only owner can change the owner
    /** @notice postcondition msg.sender == __verifier_old(owner) || owner == __verifier_old(owner) */
    function changeOwnerCorrect(address newOwner) public {
        require(msg.sender == owner);
        owner = newOwner;
    }

    // Incorrect version, anyone can change the owner
    /** @notice postcondition msg.sender == __verifier_old(owner) || owner == __verifier_old(owner) */
    function changeOwnerIncorrect(address newOwner) public {
        owner = newOwner;
    }

    /** @notice postcondition counter == __verifier_old(counter) + 1 */
    function increment() public {
        counter++;
    }

}