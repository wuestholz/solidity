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

type string_t;
// 
// ------- Source: Specification.sol -------
// Pragma: solidity^0.4.23
// 
// ------- Contract: SomeContract -------
// 
// State variable: x : uint256
var x#3: [address_t]int;
// 
// State variable: y : uint256
var y#5: [address_t]int;
// 
// Function:  : function ()
procedure __constructor#15(__this: address_t, __msg_sender: address_t, __msg_value: int)
{
  y#5 := y#5[__this := 0];
  x#3 := x#3[__this := y#5[__this]];
  $return0:
}

// 
// Function: add : function (uint256)
procedure add#29(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#17: int)
{
  x#3 := x#3[__this := (x#3[__this] + amount#17)];
  y#5 := y#5[__this := (y#5[__this] + amount#17)];
  $return1:
}

