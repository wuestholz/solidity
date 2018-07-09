pragma solidity ^0.4.23;

contract Mappings {
    mapping(address=>uint) user_balances;

    function getMyBalance() view public returns (uint) {
        return user_balances[msg.sender];
    }
}