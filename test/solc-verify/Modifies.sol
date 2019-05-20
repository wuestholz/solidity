pragma solidity >=0.5.0;

contract Simple {
    mapping(address=>int) xs;
    address owner;
    int counter;

    constructor() public {
        owner = msg.sender;
    }

    /**
    * @notice modifies xs[msg.sender]
    * @notice modifies counter when msg.sender == owner
    */
    function set(int x) public {
        xs[msg.sender] = x;
        if (msg.sender == owner)
            counter++;
    }
    // xs:      ensures xs[msg.sender := default] == old(xs)[msg.sender := default]
    // owner:   ensures old(owner) == owner
    // counter: ensures msg.sender == owner OR counter == old(counter)


    function get() public view returns (int) {
        return xs[msg.sender];
    }

}