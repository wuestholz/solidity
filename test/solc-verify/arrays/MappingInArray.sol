pragma solidity >=0.5.0;

contract MappingInArray {
    mapping(address=>int)[2] arr;

    constructor() public {
        assert(arr[0][msg.sender] == 0);
    }

    function() external payable {
        arr[0][msg.sender] = 5;
        int x = arr[0][msg.sender];
        assert(x == 5);
    }
}