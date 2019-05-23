pragma solidity >=0.5.0;

contract Simple {
    mapping(address=>int) xs;
    address owner;
    int counter;

    constructor() public {
        owner = msg.sender;
    }

    /** @notice modifies owner */
    function changeOwner() public {
        require(msg.sender == owner);
        owner = msg.sender;
    }

    /**
    * @notice modifies xs
    * @notice modifies counter
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