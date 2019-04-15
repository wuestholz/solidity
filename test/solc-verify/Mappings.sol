pragma solidity >=0.5.0;

contract Mappings {
    mapping(address=>uint) user_balances;

    function getMyBalance() view public returns (uint) {
        return user_balances[msg.sender];
    }

    function setMyBalance(uint newBalance) public {
        user_balances[msg.sender] = newBalance;
    }
}

contract Nested {
    mapping (address => mapping (address => uint)) nestedMap;

    function getSomething(address idx1, address idx2) view public returns (uint) {
        return nestedMap[idx1][idx2];
    }

    function setSomething(address idx1, address idx2, uint value) public {
        nestedMap[idx1][idx2] = value;
    }
}