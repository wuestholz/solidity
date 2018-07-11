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
// ------- Source: Assignments.sol -------
// Pragma: solidity^0.4.23
// 
// ------- Contract: LocalVars -------
// 
// State variable: contractVar : uint256
var contractVar#3: [address_t]int;
// 
// Function: init : function ()
procedure init#11(__this: address_t, __msg_sender: address_t, __msg_value: int)
{
  contractVar#3 := contractVar#3[__this := 10];
  $11end:
}

// 
// Function: doSomething : function (uint256) returns (uint256)
procedure doSomething#33(__this: address_t, __msg_sender: address_t, __msg_value: int, param#13: int)
  returns (#16: int)
{
  var localVar#19: int;
  localVar#19 := 5;
  localVar#19 := (localVar#19 + param#13);
  contractVar#3 := contractVar#3[__this := (contractVar#3[__this] + localVar#19)];
  #16 := contractVar#3[__this];
  goto $33end;
  $33end:
}

// 
// Function: chained : function (uint256) returns (uint256)
procedure chained#54(__this: address_t, __msg_sender: address_t, __msg_value: int, param#35: int)
  returns (#38: int)
{
  var localVar#41: int;
  contractVar#3 := contractVar#3[__this := (param#35 + 1)];
  localVar#41 := contractVar#3[__this];
  #38 := localVar#41;
  goto $54end;
  $54end:
}

// 
// Function: chained2 : function (uint256) returns (uint256)
procedure chained2#74(__this: address_t, __msg_sender: address_t, __msg_value: int, param#56: int)
  returns (#59: int)
{
  var localVar#62: int;
  localVar#62 := param#56;
  localVar#62 := (localVar#62 + 1);
  contractVar#3 := contractVar#3[__this := localVar#62];
  #59 := contractVar#3[__this];
  goto $74end;
  $74end:
}

// 
// Function: incrDecr : function (uint256) pure returns (uint256)
procedure incrDecr#136(__this: address_t, __msg_sender: address_t, __msg_value: int, param#76: int)
  returns (#79: int)
{
  var localVar#82: int;
  var x#86: int;
  var inc#88: int;
  var y#99: int;
  var inc#101: int;
  var z#110: int;
  var dec#112: int;
  var t#123: int;
  var dec#125: int;
  localVar#82 := param#76;
  inc#88 := localVar#82;
  localVar#82 := (localVar#82 + 1);
  x#86 := inc#88;
  assert (x#86 == (localVar#82 - 1));
  localVar#82 := (localVar#82 + 1);
  inc#101 := localVar#82;
  y#99 := inc#101;
  assert (y#99 == localVar#82);
  dec#112 := localVar#82;
  localVar#82 := (localVar#82 - 1);
  z#110 := dec#112;
  assert (z#110 == (localVar#82 + 1));
  localVar#82 := (localVar#82 - 1);
  dec#125 := localVar#82;
  t#123 := dec#125;
  assert (t#123 == localVar#82);
  #79 := localVar#82;
  goto $136end;
  $136end:
}

// 
// Function: incrContractVar : function () returns (uint256)
procedure incrContractVar#168(__this: address_t, __msg_sender: address_t, __msg_value: int)
  returns (#139: int)
{
  var x#142: int;
  var inc#144: int;
  var y#155: int;
  var inc#157: int;
  inc#144 := contractVar#3[__this];
  contractVar#3 := contractVar#3[__this := (contractVar#3[__this] + 1)];
  x#142 := inc#144;
  assert (x#142 == (contractVar#3[__this] - 1));
  contractVar#3 := contractVar#3[__this := (contractVar#3[__this] + 1)];
  inc#157 := contractVar#3[__this];
  y#155 := inc#157;
  assert (y#155 == contractVar#3[__this]);
  #139 := contractVar#3[__this];
  goto $168end;
  $168end:
}

// 
// Function: complexStuff : function (uint256) pure returns (uint256)
procedure complexStuff#188(__this: address_t, __msg_sender: address_t, __msg_value: int, param#170: int)
  returns (#173: int)
{
  var x#176: int;
  var inc#180: int;
  var inc#183: int;
  x#176 := param#170;
  x#176 := (x#176 + 1);
  inc#180 := x#176;
  inc#183 := x#176;
  x#176 := (x#176 + 1);
  #173 := (inc#180 + inc#183);
  goto $188end;
  $188end:
}

// 
// Function: __verifier_main : function ()
procedure main(__this: address_t, __msg_sender: address_t, __msg_value: int)
{
  var doSomething#33#197: int;
  var doSomething#33#205: int;
  var chained#54#213: int;
  var chained2#74#221: int;
  var incrDecr#136#229: int;
  var incrContractVar#168#239: int;
  var complexStuff#188#247: int;
  call init#11(__this, __this, 0);
  call doSomething#33#197 := doSomething#33(__this, __this, 0, 20);
  assert (doSomething#33#197 == 35);
  call doSomething#33#205 := doSomething#33(__this, __this, 0, 10);
  assert (doSomething#33#205 == 50);
  call chained#54#213 := chained#54(__this, __this, 0, 5);
  assert (chained#54#213 == 6);
  call chained2#74#221 := chained2#74(__this, __this, 0, 5);
  assert (chained2#74#221 == 6);
  call incrDecr#136#229 := incrDecr#136(__this, __this, 0, 5);
  assert (incrDecr#136#229 == 5);
  call init#11(__this, __this, 0);
  call incrContractVar#168#239 := incrContractVar#168(__this, __this, 0);
  assert (incrContractVar#168#239 == 12);
  call complexStuff#188#247 := complexStuff#188(__this, __this, 0, 0);
  assert (complexStuff#188#247 == 2);
  $253end:
}

