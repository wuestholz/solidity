// Global declarations and definitions related to the address type
type address_t;
var __balance: [address_t]int;
procedure __transfer(__this: address_t, __msg_sender: address_t, __msg_value: int, amount: int)
  requires (__balance[__msg_sender] >= amount);
  ensures ((__this != __msg_sender) ==> (__balance[__msg_sender] == (old(__balance[__msg_sender]) - amount)));
  ensures ((__this != __msg_sender) ==> (__balance[__this] == (old(__balance[__this]) + amount)));
  ensures ((__this == __msg_sender) ==> (__balance[__msg_sender] == old(__balance[__msg_sender])));
  ensures ((__this == __msg_sender) ==> (__balance[__this] == old(__balance[__this])));
{
  __balance := __balance[__this := (__balance[__this] + amount)];
  __balance := __balance[__msg_sender := (__balance[__msg_sender] - amount)];
  // TODO: call fallback, exception handling
}

procedure __call(__this: address_t, __msg_sender: address_t, __msg_value: int)
  returns (__result: bool)
{
  // TODO: model something nondeterministic here
}

procedure __send(__this: address_t, __msg_sender: address_t, __msg_value: int, amount: int)
  returns (__result: bool)
  requires (__balance[__msg_sender] >= amount);
  ensures ((__result && (__this != __msg_sender)) ==> (__balance[__msg_sender] == (old(__balance[__msg_sender]) - amount)));
  ensures ((__result && (__this != __msg_sender)) ==> (__balance[__this] == (old(__balance[__this]) + amount)));
  ensures ((!(__result) || (__this == __msg_sender)) ==> (__balance[__msg_sender] == old(__balance[__msg_sender])));
  ensures ((!(__result) || (__this == __msg_sender)) ==> (__balance[__this] == old(__balance[__this])));
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

// 
// ------- Source: Arithmetic.sol -------
// Pragma: solidity^0.4.23
// 
// ------- Contract: SimpleArithmetic -------
// 
// Function: and : function (bool,bool) pure returns (bool)
procedure and#15(__this: address_t, __msg_sender: address_t, __msg_value: int, a#3: bool, b#5: bool)
  returns (#8: bool)
{
  #8 := (a#3 && b#5);
  return;
}

// 
// Function: or : function (bool,bool) pure returns (bool)
procedure or#29(__this: address_t, __msg_sender: address_t, __msg_value: int, a#17: bool, b#19: bool)
  returns (#22: bool)
{
  #22 := (a#17 || b#19);
  return;
}

// 
// Function: not : function (bool) pure returns (bool)
procedure not#40(__this: address_t, __msg_sender: address_t, __msg_value: int, a#31: bool)
  returns (#34: bool)
{
  #34 := !(a#31);
  return;
}

// 
// Function: add : function (uint256,uint256) pure returns (uint256)
procedure add#54(__this: address_t, __msg_sender: address_t, __msg_value: int, a#42: int, b#44: int)
  returns (#47: int)
{
  #47 := (a#42 + b#44);
  return;
}

// 
// Function: sub : function (uint256,uint256) pure returns (uint256)
procedure sub#68(__this: address_t, __msg_sender: address_t, __msg_value: int, a#56: int, b#58: int)
  returns (#61: int)
{
  #61 := (a#56 - b#58);
  return;
}

// 
// Function: mul : function (uint256,uint256) pure returns (uint256)
procedure mul#82(__this: address_t, __msg_sender: address_t, __msg_value: int, a#70: int, b#72: int)
  returns (#75: int)
{
  #75 := (a#70 * b#72);
  return;
}

// 
// Function: div : function (uint256,uint256) pure returns (uint256)
procedure div#96(__this: address_t, __msg_sender: address_t, __msg_value: int, a#84: int, b#86: int)
  returns (#89: int)
{
  #89 := (a#84 div b#86);
  return;
}

// 
// Function: mod : function (uint256,uint256) pure returns (uint256)
procedure mod#110(__this: address_t, __msg_sender: address_t, __msg_value: int, a#98: int, b#100: int)
  returns (#103: int)
{
  #103 := (a#98 mod b#100);
  return;
}

// 
// Function: minus : function (int256) pure returns (int256)
procedure minus#121(__this: address_t, __msg_sender: address_t, __msg_value: int, a#112: int)
  returns (#115: int)
{
  #115 := -(a#112);
  return;
}

// 
// Function: doSomethingComplex : function (uint256,uint256,uint256) pure returns (uint256)
procedure doSomethingComplex#145(__this: address_t, __msg_sender: address_t, __msg_value: int, a#123: int, b#125: int, c#127: int)
  returns (#130: int)
{
  #130 := ((((a#123 + b#125) * 1234) div c#127) mod 56);
  return;
}

// 
// Function: assignAdd : function (uint256,uint256) pure returns (uint256)
procedure assignAdd#165(__this: address_t, __msg_sender: address_t, __msg_value: int, a#147: int, b#149: int)
  returns (#152: int)
{
  var result#155: int;
  result#155 := a#147;
  result#155 := (result#155 + b#149);
  #152 := result#155;
  return;
}

// 
// Function: assignSub : function (uint256,uint256) pure returns (uint256)
procedure assignSub#185(__this: address_t, __msg_sender: address_t, __msg_value: int, a#167: int, b#169: int)
  returns (#172: int)
{
  var result#175: int;
  result#175 := a#167;
  result#175 := (result#175 - b#169);
  #172 := result#175;
  return;
}

// 
// Function: assignMul : function (uint256,uint256) pure returns (uint256)
procedure assignMul#205(__this: address_t, __msg_sender: address_t, __msg_value: int, a#187: int, b#189: int)
  returns (#192: int)
{
  var result#195: int;
  result#195 := a#187;
  result#195 := (result#195 * b#189);
  #192 := result#195;
  return;
}

// 
// Function: assignDiv : function (uint256,uint256) pure returns (uint256)
procedure assignDiv#225(__this: address_t, __msg_sender: address_t, __msg_value: int, a#207: int, b#209: int)
  returns (#212: int)
{
  var result#215: int;
  result#215 := a#207;
  result#215 := (result#215 div b#209);
  #212 := result#215;
  return;
}

// 
// Function: assignMod : function (uint256,uint256) pure returns (uint256)
procedure assignMod#245(__this: address_t, __msg_sender: address_t, __msg_value: int, a#227: int, b#229: int)
  returns (#232: int)
{
  var result#235: int;
  result#235 := a#227;
  result#235 := (result#235 mod b#229);
  #232 := result#235;
  return;
}

// 
// Function: eq : function (uint256,uint256) pure returns (bool)
procedure eq#259(__this: address_t, __msg_sender: address_t, __msg_value: int, a#247: int, b#249: int)
  returns (#252: bool)
{
  #252 := (a#247 == b#249);
  return;
}

// 
// Function: neq : function (uint256,uint256) pure returns (bool)
procedure neq#273(__this: address_t, __msg_sender: address_t, __msg_value: int, a#261: int, b#263: int)
  returns (#266: bool)
{
  #266 := (a#261 != b#263);
  return;
}

// 
// Function: lt : function (uint256,uint256) pure returns (bool)
procedure lt#287(__this: address_t, __msg_sender: address_t, __msg_value: int, a#275: int, b#277: int)
  returns (#280: bool)
{
  #280 := (a#275 < b#277);
  return;
}

// 
// Function: gt : function (uint256,uint256) pure returns (bool)
procedure gt#301(__this: address_t, __msg_sender: address_t, __msg_value: int, a#289: int, b#291: int)
  returns (#294: bool)
{
  #294 := (a#289 > b#291);
  return;
}

// 
// Function: lte : function (uint256,uint256) pure returns (bool)
procedure lte#315(__this: address_t, __msg_sender: address_t, __msg_value: int, a#303: int, b#305: int)
  returns (#308: bool)
{
  #308 := (a#303 <= b#305);
  return;
}

// 
// Function: gte : function (uint256,uint256) pure returns (bool)
procedure gte#329(__this: address_t, __msg_sender: address_t, __msg_value: int, a#317: int, b#319: int)
  returns (#322: bool)
{
  #322 := (a#317 >= b#319);
  return;
}

// 
// Function: __verifier_main : function () pure
procedure main(__this: address_t, __msg_sender: address_t, __msg_value: int)
{
  var and#15#336: bool;
  var and#15#345: bool;
  var and#15#354: bool;
  var and#15#363: bool;
  var or#29#372: bool;
  var or#29#381: bool;
  var or#29#390: bool;
  var or#29#399: bool;
  var not#40#407: bool;
  var not#40#415: bool;
  var add#54#424: int;
  var sub#68#433: int;
  var mul#82#442: int;
  var div#96#451: int;
  var mod#110#460: int;
  var minus#121#468: int;
  var minus#121#478: int;
  var assignAdd#165#487: int;
  var assignSub#185#496: int;
  var assignMul#205#505: int;
  var assignDiv#225#514: int;
  var assignMod#245#523: int;
  var eq#259#532: bool;
  var eq#259#541: bool;
  var neq#273#550: bool;
  var neq#273#559: bool;
  var lt#287#568: bool;
  var lt#287#577: bool;
  var lt#287#586: bool;
  var gt#301#595: bool;
  var gt#301#604: bool;
  var gt#301#613: bool;
  var lte#315#622: bool;
  var lte#315#631: bool;
  var lte#315#640: bool;
  var gte#329#649: bool;
  var gte#329#658: bool;
  var gte#329#667: bool;
  call and#15#336 := and#15(__this, __this, 0, true, true);
  assert (and#15#336 == true);
  call and#15#345 := and#15(__this, __this, 0, true, false);
  assert (and#15#345 == false);
  call and#15#354 := and#15(__this, __this, 0, false, true);
  assert (and#15#354 == false);
  call and#15#363 := and#15(__this, __this, 0, false, false);
  assert (and#15#363 == false);
  call or#29#372 := or#29(__this, __this, 0, true, true);
  assert (or#29#372 == true);
  call or#29#381 := or#29(__this, __this, 0, true, false);
  assert (or#29#381 == true);
  call or#29#390 := or#29(__this, __this, 0, false, true);
  assert (or#29#390 == true);
  call or#29#399 := or#29(__this, __this, 0, false, false);
  assert (or#29#399 == false);
  call not#40#407 := not#40(__this, __this, 0, true);
  assert (not#40#407 == false);
  call not#40#415 := not#40(__this, __this, 0, false);
  assert (not#40#415 == true);
  call add#54#424 := add#54(__this, __this, 0, 1, 2);
  assert (add#54#424 == 3);
  call sub#68#433 := sub#68(__this, __this, 0, 5, 2);
  assert (sub#68#433 == 3);
  call mul#82#442 := mul#82(__this, __this, 0, 5, 2);
  assert (mul#82#442 == 10);
  call div#96#451 := div#96(__this, __this, 0, 7, 2);
  assert (div#96#451 == 3);
  call mod#110#460 := mod#110(__this, __this, 0, 10, 3);
  assert (mod#110#460 == 1);
  call minus#121#468 := minus#121(__this, __this, 0, 5);
  assert (minus#121#468 == -(5));
  call minus#121#478 := minus#121(__this, __this, 0, -(3));
  assert (minus#121#478 == 3);
  call assignAdd#165#487 := assignAdd#165(__this, __this, 0, 1, 2);
  assert (assignAdd#165#487 == 3);
  call assignSub#185#496 := assignSub#185(__this, __this, 0, 5, 2);
  assert (assignSub#185#496 == 3);
  call assignMul#205#505 := assignMul#205(__this, __this, 0, 5, 2);
  assert (assignMul#205#505 == 10);
  call assignDiv#225#514 := assignDiv#225(__this, __this, 0, 7, 2);
  assert (assignDiv#225#514 == 3);
  call assignMod#245#523 := assignMod#245(__this, __this, 0, 10, 3);
  assert (assignMod#245#523 == 1);
  call eq#259#532 := eq#259(__this, __this, 0, 1, 1);
  assert (eq#259#532 == true);
  call eq#259#541 := eq#259(__this, __this, 0, 1, 2);
  assert (eq#259#541 == false);
  call neq#273#550 := neq#273(__this, __this, 0, 1, 1);
  assert (neq#273#550 == false);
  call neq#273#559 := neq#273(__this, __this, 0, 1, 2);
  assert (neq#273#559 == true);
  call lt#287#568 := lt#287(__this, __this, 0, 1, 1);
  assert (lt#287#568 == false);
  call lt#287#577 := lt#287(__this, __this, 0, 1, 2);
  assert (lt#287#577 == true);
  call lt#287#586 := lt#287(__this, __this, 0, 2, 1);
  assert (lt#287#586 == false);
  call gt#301#595 := gt#301(__this, __this, 0, 1, 1);
  assert (gt#301#595 == false);
  call gt#301#604 := gt#301(__this, __this, 0, 1, 2);
  assert (gt#301#604 == false);
  call gt#301#613 := gt#301(__this, __this, 0, 2, 1);
  assert (gt#301#613 == true);
  call lte#315#622 := lte#315(__this, __this, 0, 1, 1);
  assert (lte#315#622 == true);
  call lte#315#631 := lte#315(__this, __this, 0, 1, 2);
  assert (lte#315#631 == true);
  call lte#315#640 := lte#315(__this, __this, 0, 2, 1);
  assert (lte#315#640 == false);
  call gte#329#649 := gte#329(__this, __this, 0, 1, 1);
  assert (gte#329#649 == true);
  call gte#329#658 := gte#329(__this, __this, 0, 1, 2);
  assert (gte#329#658 == false);
  call gte#329#667 := gte#329(__this, __this, 0, 2, 1);
  assert (gte#329#667 == true);
}

