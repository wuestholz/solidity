pragma solidity >=0.5.0;

library L {
    function msgsender() internal view returns(address) {
        return msg.sender;
    }
    function msgvalue() internal view returns(uint) {
        return msg.value;
    }
}

contract Issue025 {
    function() external payable {
        assert(L.msgsender() == msg.sender);
        assert(L.msgvalue() == msg.value);
    }
}