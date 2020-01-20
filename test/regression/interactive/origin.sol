
// RUN: %solc %s --c-model --contract-list B --output-dir=%t
// RUN: cd %t
// RUN: cmake -DSEA_PATH=%seapath
// RUN: make icmodel
// RUN: echo 1 2 3 1 0 0 1 1 0 0 0 | ./icmodel --return-0 --count-transactions 2>&1 | OutputCheck %s --comment=//
// RUN: echo 1 2 3 1 0 0 1 1 0 0 1 0 | ./icmodel
// CHECK: assert
// CHECK: Transaction Count: 1

/*
 * Tests that model permits interactions with each contract.
 */

contract A {
    function g() public view {
        assert(msg.sender != tx.origin);
    }
}

contract B {
    A a;

    constructor() public {
        a = new A();
    }

    function f() public view {
        assert(msg.sender == tx.origin);
        a.g();
    }
}