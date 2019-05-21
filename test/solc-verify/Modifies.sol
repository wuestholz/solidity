pragma solidity >=0.5.0;

contract Simple {
    mapping(address=>int) xs;
    address owner;
    int counter;

    constructor() public {
        owner = msg.sender;
    }

    /** @notice modifies owner if msg.sender == old(owner) */
    function changeOwner() public {
        require(msg.sender == owner);
        owner = msg.sender;
    }
    // ensures msg.sender == old(owner) OR owner == old(owner)

    /**
    * @notice modifies xs[msg.sender]
    * @notice modifies counter if msg.sender == owner
    * @notice postcondition msg.sender == owner ==> counter = old(counter) + 1
    */
    function set(int x) public {
        xs[msg.sender] = x;
        if (msg.sender == owner)
            counter++;
    }
    // xs:      ensures xs[msg.sender := default] == old(xs)[msg.sender := default]
    // owner:   ensures old(owner) == owner
    // counter: ensures msg.sender == owner OR counter == old(counter)


    // With postcondition and old():
    // xs:      ???
    // owner:   @notice postcondition old(owner) == owner
    // counter: @notice postcondition msg.sender != owner ==> counter == old(counter)


    function get() public view returns (int) {
        return xs[msg.sender];
    }

}