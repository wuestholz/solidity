pragma solidity ^0.4.23;

contract BitPreciseArith {
    function add8u(uint8 x, uint8 y) private pure returns (uint8) { return x + y; }
    function sub8u(uint8 x, uint8 y) private pure returns (uint8) { return x - y; }
    function mul8u(uint8 x, uint8 y) private pure returns (uint8) { return x * y; }
    function div8u(uint8 x, uint8 y) private pure returns (uint8) { return x / y; }

    function add8s(int8 x, int8 y) private pure returns (int8) { return x + y; }
    function sub8s(int8 x, int8 y) private pure returns (int8) { return x - y; }
    function mul8s(int8 x, int8 y) private pure returns (int8) { return x * y; }
    function div8s(int8 x, int8 y) private pure returns (int8) { return x / y; }

    function preinc8u(uint8 x) private pure returns (uint8) {
        uint8 result = x;
        uint8 tmp = ++result;
        assert(tmp == result);
        return result;
    }
    function postinc8u(uint8 x) private pure returns (uint8) {
        uint8 result = x;
        uint8 tmp = result++;
        assert(tmp == x);
        return result;
    }
    function predec8u(uint8 x) private pure returns (uint8) {
        uint8 result = x;
        uint8 tmp = --result;
        assert(tmp == result);
        return result;
    }
    function postdec8u(uint8 x) private pure returns (uint8) {
        uint8 result = x;
        uint8 tmp = result--;
        assert(tmp == x);
        return result;
    }

    function bitand8u(uint8 x, uint8 y) private pure returns (uint8) { return x & y; }
    function bitor8u(uint8 x, uint8 y) private pure returns (uint8) { return x | y; }
    function bitxor8u(uint8 x, uint8 y) private pure returns (uint8) { return x ^ y; }
    function bitshl8u(uint8 x, uint8 y) private pure returns (uint8) { return x << y; }
    function bitsar8u(uint8 x, uint8 y) private pure returns (uint8) { return x >> y; }
    function bitsar8s(int8 x, int8 y) private pure returns (int8) { return x >> y; }

    function eq8u(uint8 x, uint8 y) private pure returns (bool) { return x == y; }
    function ne8u(uint8 x, uint8 y) private pure returns (bool) { return x != y; }
    function lt8u(uint8 x, uint8 y) private pure returns (bool) { return x < y; }
    function le8u(uint8 x, uint8 y) private pure returns (bool) { return x <= y; }
    function gt8u(uint8 x, uint8 y) private pure returns (bool) { return x > y; }
    function ge8u(uint8 x, uint8 y) private pure returns (bool) { return x >= y; }

    function __verifier_main() public pure {
        assert(add8u(128, 127) == 255);
        assert(add8u(128, 128) == 0);
        assert(sub8u(5, 3) == 2);
        assert(sub8u(0, 1) == 255);
        assert(mul8u(5, 7) == 35);
        assert(mul8u(8, 32) == 0);
        assert(div8u(48, 6) == 8);
        assert(div8u(3, 6) == 0);

        assert(add8s(127, -1) == 126);
        assert(add8s(127, 1) == -128);
        assert(sub8s(127, -1) == -128);
        assert(sub8s(127, 1) == 126);
        assert(mul8s(5, -3) == - 15);
        assert(mul8s(8, 16) == -128);
        assert(div8s(48, 6) == 8);
        assert(div8s(48, -6) == -8);
        assert(div8s(-128, -1) == -128);

        assert(preinc8u(1) == 2);
        assert(postinc8u(1) == 2);
        assert(predec8u(5) == 4);
        assert(postdec8u(5) == 4);

        assert(bitand8u(123, 45) == 41);
        assert(bitor8u(123, 45) == 127);
        assert(bitxor8u(123, 45) == 86);
        assert(bitshl8u(3, 2) == 12);
        assert(bitshl8u(3, 100) == 0);
        assert(bitsar8u(48, 4) == 3);
        assert(bitsar8u(48, 100) == 0);
        assert(bitsar8s(-48, 4) == -3);
        //assert(bitsar8s(-48, 6) == 0); // TODO: different semantics

        assert(eq8u(34, 34) == true);
        assert(eq8u(34, 35) == false);
        assert(eq8u(0, add8u(128, 128)) == true);
        assert(ne8u(34, 34) == false);
        assert(ne8u(34, 35) == true);
        assert(lt8u(34, 35) == true);
        assert(lt8u(34, 34) == false);
        assert(lt8u(34, 33) == false);
        assert(le8u(34, 35) == true);
        assert(le8u(34, 34) == true);
        assert(le8u(34, 33) == false);
        assert(gt8u(34, 35) == false);
        assert(gt8u(34, 34) == false);
        assert(gt8u(34, 33) == true);
        assert(ge8u(34, 35) == false);
        assert(ge8u(34, 34) == true);
        assert(ge8u(34, 33) == true);
    }
}