pragma solidity >=0.5.0;

/**
 * @notice invariant x == y
 */
contract SpecificationPrivateFunctions {
    uint x;
    uint y;

    function add(uint amount) public {
        // Private functions do not have the invariant as pre/postcondition
        // so they should be inlined
        addToX(amount);
        addToY(amount);
    }

    function addWrong(uint amount) public {
        addToXWrong(amount); // ERROR
        addToY(amount);
    }

    function addToX(uint amount) private {
        x += amount;
    }

    function addToXWrong(uint amount) private {
        x -= amount;
    }

    function addToY(uint amount) private {
        y += amount;
    }
}