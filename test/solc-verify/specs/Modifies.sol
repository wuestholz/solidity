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
    * @notice modifies xs[msg.sender]
    * @notice modifies counter if msg.sender == __verifier_old_address(owner)
    * @notice postcondition msg.sender != owner || counter == __verifier_old_int(counter) + 1
    */
    function setCorrect(int x) public {
        xs[msg.sender] = x;
        if (msg.sender == owner)
            counter++;
    }

    /**
    * @notice modifies xs[at]
    */
    function setCorrect2(address at, int x) public {
        xs[at] = x;
    }

    /**
    * @notice modifies xs[msg.sender]
    */
    function setInorrect(int x) public {
        xs[address(this)] = x;
    }

    /**
    * @notice modifies xs[msg.sender] if x > 0
    */
    function setInorrect2(int x) public {
        xs[address(this)] = x;
    }

    function get() public view returns (int) {
        return xs[msg.sender];
    }

    /** @notice modifies * */
    function all() public {
        xs[msg.sender] = 0;
        owner = msg.sender;
        counter = 0;
    }

}