// Global declarations and definitions related to the address type
type address_t;
const unique __zero__address: address_t;
var __balance: [address_t]int;
procedure __transfer(__this: address_t, __msg_sender: address_t, __msg_value: int, amount: int)
  requires (__balance[__msg_sender] >= amount);

  ensures if (__this != __msg_sender) then ((__balance[__msg_sender] == (old(__balance[__msg_sender]) - amount)) && (__balance[__this] == (old(__balance[__this]) + amount))) else ((__balance[__msg_sender] == old(__balance[__msg_sender])) && (__balance[__this] == old(__balance[__this])));

{
  __balance := __balance[__this := (__balance[__this] + amount)];
  __balance := __balance[__msg_sender := (__balance[__msg_sender] - amount)];
  // TODO: call fallback, exception handling
}

procedure __call(__this: address_t, __msg_sender: address_t, __msg_value: int)
  returns (__result: bool)
  ensures (__result || (__balance == old(__balance)));

{
  // TODO: call fallback
  if (*) {
  __balance := __balance[__this := (__balance[__this] + __msg_value)];
  __result := true;
  }
  else {
  __result := false;
  }

}

procedure __send(__this: address_t, __msg_sender: address_t, __msg_value: int, amount: int)
  returns (__result: bool)
  requires (__balance[__msg_sender] >= amount);

  ensures if (__result && (__this != __msg_sender)) then ((__balance[__msg_sender] == (old(__balance[__msg_sender]) - amount)) && (__balance[__this] == (old(__balance[__this]) + amount))) else ((__balance[__msg_sender] == old(__balance[__msg_sender])) && (__balance[__this] == old(__balance[__this])));

{
  // TODO: call fallback
  if (*) {
  __balance := __balance[__this := (__balance[__this] + amount)];
  __balance := __balance[__msg_sender := (__balance[__msg_sender] - amount)];
  __result := true;
  }
  else {
  __result := false;
  }

}

type string_t;
var __now: int;
// 
// ------- Source: BitPreciseExt.sol -------
// Pragma: solidity^0.4.23
// 
// ------- Contract: BitPreciseExt -------
// 
// Function: u32tou40 : function (uint32) pure returns (uint40)
procedure {:inline 1} u32tou40#11(__this: address_t, __msg_sender: address_t, __msg_value: int, x#3: bv32)
  returns (#6: bv40)
{
  #6 := bvzeroext32to40(x#3);
  goto $return0;
  $return0:
}

// 
// Function: s16tos48 : function (int16) pure returns (int48)
procedure {:inline 1} s16tos48#25(__this: address_t, __msg_sender: address_t, __msg_value: int, x#13: bv16)
  returns (#16: bv48)
{
  var y#19: bv48;
  y#19 := bvsignext16to48(x#13);
  #16 := y#19;
  goto $return1;
  $return1:
}

// 
// Function: u32tos40 : function (uint32) pure returns (int40)
procedure {:inline 1} u32tos40#39(__this: address_t, __msg_sender: address_t, __msg_value: int, x#27: bv32)
  returns (#30: bv40)
{
  var y#33: bv40;
  y#33 := bvzeroext32to40(x#27);
  #30 := y#33;
  goto $return2;
  $return2:
}

// 
// Function: __verifier_main : function () pure
procedure main(__this: address_t, __msg_sender: address_t, __msg_value: int)
{
  var u32tou40#11#45: bv40;
  var s16tos48#25#53: bv48;
  var s16tos48#25#62: bv48;
  var u32tos40#39#71: bv40;
  assume {:sourceloc "BitPreciseExt.sol", 19, 16} {:message ""} true;
  call u32tou40#11#45 := u32tou40#11(__this, __this, 0, 123bv32);
  assert {:sourceloc "BitPreciseExt.sol", 19, 9} {:message "Assertion might not hold."} (u32tou40#11#45 == 123bv40);
  assume {:sourceloc "BitPreciseExt.sol", 20, 16} {:message ""} true;
  call s16tos48#25#53 := s16tos48#25(__this, __this, 0, 123bv16);
  assert {:sourceloc "BitPreciseExt.sol", 20, 9} {:message "Assertion might not hold."} (s16tos48#25#53 == 123bv48);
  assume {:sourceloc "BitPreciseExt.sol", 21, 16} {:message ""} true;
  call s16tos48#25#62 := s16tos48#25(__this, __this, 0, bv16neg(123bv16));
  assert {:sourceloc "BitPreciseExt.sol", 21, 9} {:message "Assertion might not hold."} (s16tos48#25#62 == bv48neg(123bv48));
  assume {:sourceloc "BitPreciseExt.sol", 22, 16} {:message ""} true;
  call u32tos40#39#71 := u32tos40#39(__this, __this, 0, 123bv32);
  assert {:sourceloc "BitPreciseExt.sol", 22, 9} {:message "Assertion might not hold."} (u32tos40#39#71 == 123bv40);
  $return3:
}

function {:bvbuiltin "bvneg"} bv16neg(bv16) returns (bv16);
function {:bvbuiltin "bvneg"} bv48neg(bv48) returns (bv48);
function {:bvbuiltin "sign_extend 32"} bvsignext16to48(bv16) returns (bv48);
function {:bvbuiltin "zero_extend 8"} bvzeroext32to40(bv32) returns (bv40);
