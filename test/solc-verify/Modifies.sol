pragma solidity >=0.5.0;

contract Simple {
    mapping(address=>int) xs;
    address owner;
    int counter;

    constructor() public {
        owner = msg.sender;
    }

    /** @notice modifies owner if msg.sender == __verifier_old_address(owner)  */
    function changeOwner(address newOwner) public {
        require(msg.sender == owner);
        owner = newOwner;
    }

    /** @notice modifies owner if msg.sender == __verifier_old_address(owner)  */
    function changeOwnerIncorrect(address newOwner) public {
        owner = newOwner;
    }

    /**
    * @notice modifies xs
    * @notice modifies counter if msg.sender == __verifier_old_address(owner)
    * @notice postcondition msg.sender != owner || counter == __verifier_old_int(counter) + 1
    */
    function set(int x) public {
        xs[msg.sender] = x;
        if (msg.sender == owner)
            counter++;
    }

    function get() public view returns (int) {
        return xs[msg.sender];
    }

}