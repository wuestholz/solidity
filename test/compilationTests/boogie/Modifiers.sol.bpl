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
// ------- Source: Modifiers.sol -------
// Pragma: solidity^0.4.23
// 
// ------- Contract: A -------
// 
// State variable: owner : address
var owner#3: [address_t]address_t;
// 
// State variable: x : uint256
var x#5: [address_t]int;
// 
// Function: someFunc : function (uint256) returns (uint256)
procedure someFunc#47(__this: address_t, __msg_sender: address_t, __msg_value: int, y#33: int)
  returns (#38: int)
{
  assume (__msg_sender == owner#3[__this]);
  x#5 := x#5[__this := (x#5[__this] + y#33)];
  #38 := x#5[__this];
  goto $14end;
  $14end:
  assert true;
  $36end:
}

// 
// Function: someFunc2 : function (uint256) returns (uint256)
procedure someFunc2#63(__this: address_t, __msg_sender: address_t, __msg_value: int, y#49: int)
  returns (#54: int)
{
  if ((__msg_sender != owner#3[__this])) {
  goto $52end;
  }

  x#5 := x#5[__this := (x#5[__this] + y#49)];
  #54 := x#5[__this];
  goto $29end;
  $29end:
  $52end:
}

