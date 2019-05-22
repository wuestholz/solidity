pragma solidity >=0.5.0;

contract OldExpr {
    address owner;
    int256 counter;

    constructor() public {
        owner = msg.sender;
    }

    /** @notice postcondition msg.sender == __verifier_old_address(owner) || owner == __verifier_old_address(owner) */
    function changeOwnerCorrect() public {
        require(msg.sender == owner);
        owner = msg.sender;
    }

    /** @notice postcondition msg.sender == __verifier_old_address(owner) || owner == __verifier_old_address(owner) */
    function changeOwnerIncorrect() public {
        owner = msg.sender;
    }

    /** @notice postcondition counter == __verifier_old_int256(counter) + 1 */
    function increment() public {
        counter++;
    }

}