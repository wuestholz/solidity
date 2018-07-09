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
// ------- Source: Initializers.sol -------
// Pragma: solidity^0.4.23
// 
// ------- Contract: Initializers -------
// 
// State variable: b : int256
var b#3: [address_t]int;
// 
// State variable: c : int256
var c#5: [address_t]int;
// 
// State variable: a : int256
var a#26: [address_t]int;
// 
// Function:  : function (int256)
procedure __constructor#15(__this: address_t, __msg_sender: address_t, __msg_value: int, b0#7: int)
{
  a#26 := a#26[__this := 5];
  b#3 := b#3[__this := b0#7];
}

// 
// Function: getA : function () view returns (int256)
procedure getA#23(__this: address_t, __msg_sender: address_t, __msg_value: int)
  returns (#18: int)
{
  #18 := a#26[__this];
  return;
}

// 
// ------- Contract: NoConstructorButNeeded -------
// 
// State variable: a : int256
var a#30: [address_t]int;
// 
// State variable: b : int256
var b#32: [address_t]int;
// 
// Default constructor
procedure __constructor#33(__this: address_t, __msg_sender: address_t, __msg_value: int)
{
  a#30 := a#30[__this := 5];
}

// 
// ------- Contract: NoConstructorNeeded -------
// 
// State variable: a : int256
var a#35: [address_t]int;
// 
// State variable: b : int256
var b#37: [address_t]int;
