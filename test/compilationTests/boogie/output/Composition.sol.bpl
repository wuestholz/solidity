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
// ------- Source: Composition.sol -------
// Pragma: solidity^0.4.23
// 
// ------- Contract: A -------
// 
// State variable: myVar : uint256
var myVar#3: [address_t]int;
// 
// Function: someFunc : function (uint256) view returns (uint256)
procedure someFunc#15(__this: address_t, __msg_sender: address_t, __msg_value: int, param#5: int)
  returns (#8: int)
{
  #8 := (myVar#3[__this] + param#5);
  goto $15end;
  $15end:
}

// 
// ------- Contract: B -------
// 
// State variable: other : contract A
var other#18: [address_t]address_t;
// 
// Function: callOtherFunc : function () view returns (uint256)
procedure callOtherFunc#29(__this: address_t, __msg_sender: address_t, __msg_value: int)
  returns (#21: int)
{
  var someFunc#15#26: int;
  call someFunc#15#26 := someFunc#15(other#18[__this], __this, 0, 10);
  #21 := someFunc#15#26;
  goto $29end;
  $29end:
}

// 
// Function: accessOtherMember : function () view returns (uint256)
procedure accessOtherMember#39(__this: address_t, __msg_sender: address_t, __msg_value: int)
  returns (#32: int)
{
  var myVar#3#36: int;
  myVar#3#36 := myVar#3[other#18[__this]];
  #32 := myVar#3#36;
  goto $39end;
  $39end:
}

