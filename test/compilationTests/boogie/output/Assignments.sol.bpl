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
  return;
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
  return;
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
  return;
}

// 
// Function: __verifier_main : function ()
procedure main(__this: address_t, __msg_sender: address_t, __msg_value: int)
{
  var doSomething#33#83: int;
  var doSomething#33#91: int;
  var chained#54#99: int;
  var chained2#74#107: int;
  call init#11(__this, __this, 0);
  call doSomething#33#83 := doSomething#33(__this, __this, 0, 20);
  assert (doSomething#33#83 == 35);
  call doSomething#33#91 := doSomething#33(__this, __this, 0, 10);
  assert (doSomething#33#91 == 50);
  call chained#54#99 := chained#54(__this, __this, 0, 5);
  assert (chained#54#99 == 6);
  call chained2#74#107 := chained2#74(__this, __this, 0, 5);
  assert (chained2#74#107 == 6);
}

