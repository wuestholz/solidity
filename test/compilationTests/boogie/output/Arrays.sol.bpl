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
// ------- Source: Arrays.sol -------
// Pragma: solidity^0.4.23
// 
// ------- Contract: Arrays -------
// 
// State variable: arr : uint256[] storage ref
var arr#4: [address_t][int]int;
var arr#4#length: [address_t]int;
// 
// Function: readStateArr : function (uint256) view returns (uint256)
procedure readStateArr#23(__this: address_t, __msg_sender: address_t, __msg_value: int, i#6: int)
  returns (#9: int)
{
  if ((i#6 < arr#4#length[__this])) {
  #9 := arr#4[__this][i#6];
  goto $23end;
  }

  #9 := 0;
  goto $23end;
  $23end:
}

// 
// Function: writeStateArr : function (uint256,uint256)
procedure writeStateArr#37(__this: address_t, __msg_sender: address_t, __msg_value: int, i#25: int, value#27: int)
{
  arr#4 := arr#4[__this := arr#4[__this][i#25 := value#27]];
  $37end:
}

// 
// Function: readParamArr : function (uint256[] memory,uint256) pure returns (uint256)
procedure readParamArr#59(__this: address_t, __msg_sender: address_t, __msg_value: int, paramArr#40: [int]int, paramArr#40#length: int, i#42: int)
  returns (#45: int)
{
  if ((i#42 < paramArr#40#length)) {
  #45 := paramArr#40[i#42];
  goto $59end;
  }

  #45 := 0;
  goto $59end;
  $59end:
}

// 
// Function: callWithLocalArray : function (uint256[] memory) pure returns (uint256)
procedure callWithLocalArray#73(__this: address_t, __msg_sender: address_t, __msg_value: int, paramArr#62: [int]int, paramArr#62#length: int)
  returns (#65: int)
{
  var readParamArr#59#70: int;
  call readParamArr#59#70 := readParamArr#59(__this, __this, 0, paramArr#62, paramArr#62#length, 123);
  #65 := readParamArr#59#70;
  goto $73end;
  $73end:
}

// 
// Function: callWithStateArray : function () view returns (uint256)
procedure callWithStateArray#84(__this: address_t, __msg_sender: address_t, __msg_value: int)
  returns (#76: int)
{
  var readParamArr#59#81: int;
  call readParamArr#59#81 := readParamArr#59(__this, __this, 0, arr#4[__this], arr#4#length[__this], 456);
  #76 := readParamArr#59#81;
  goto $84end;
  $84end:
}

