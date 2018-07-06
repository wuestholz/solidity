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
// ------- Source: Addresses.sol -------
// Pragma: solidity^0.4.23
// 
// ------- Contract: Addresses -------
// 
// Function: testTransfer : function (uint256)
procedure testTransfer#50(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#3: int)
{
  var oldSum#7: int;
  var newSum#33: int;
  oldSum#7 := (__balance[__this] + __balance[__msg_sender]);
  if ((__balance[__this] >= amount#3)) {
  call __transfer(__msg_sender, __this, 0, amount#3);
  }

  newSum#33 := (__balance[__this] + __balance[__msg_sender]);
  assert (oldSum#7 == newSum#33);
}

// 
// Function: testTransfer2 : function (uint256)
procedure testTransfer2#113(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#52: int)
{
  var oldThisBalance#63: int;
  var oldSenderBalance#70: int;
  assume (__this != __msg_sender);
  oldThisBalance#63 := __balance[__this];
  oldSenderBalance#70 := __balance[__msg_sender];
  if ((__balance[__this] >= amount#52)) {
  call __transfer(__msg_sender, __this, 0, amount#52);
  assert ((oldThisBalance#63 - amount#52) == __balance[__this]);
  assert ((oldSenderBalance#70 + amount#52) == __balance[__msg_sender]);
  }

}

// 
// Function: testTransferError : function (uint256)
procedure testTransferError#165(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#115: int)
{
  var oldSum#119: int;
  var newSum#145: int;
  oldSum#119 := (__balance[__this] + __balance[__msg_sender]);
  if ((__balance[__this] >= amount#115)) {
  call __transfer(__msg_sender, __this, 0, amount#115);
  }

  newSum#145 := (__balance[__this] + __balance[__msg_sender]);
  assert (oldSum#119 == (newSum#145 + 1234));
}

// 
// Function: testSend : function (uint256)
procedure testSend#214(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#167: int)
{
  var oldSum#171: int;
  var __send#193: bool;
  var newSum#197: int;
  oldSum#171 := (__balance[__this] + __balance[__msg_sender]);
  if ((__balance[__this] >= amount#167)) {
  call __send#193 := __send(__msg_sender, __this, 0, amount#167);
  }

  newSum#197 := (__balance[__this] + __balance[__msg_sender]);
  assert (oldSum#171 == newSum#197);
}

// 
// Function: testSend2 : function (uint256)
procedure testSend2#288(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#216: int)
{
  var oldThisBalance#220: int;
  var oldSenderBalance#227: int;
  var success#233: bool;
  var __send#247: bool;
  oldThisBalance#220 := __balance[__this];
  oldSenderBalance#227 := __balance[__msg_sender];
  success#233 := false;
  if ((__balance[__this] >= amount#216)) {
  call __send#247 := __send(__msg_sender, __this, 0, amount#216);
  success#233 := __send#247;
  }

  if (success#233) {
  assert ((oldThisBalance#220 + oldSenderBalance#227) == (__balance[__this] + __balance[__msg_sender]));
  }
  else {
  assert (oldThisBalance#220 == __balance[__this]);
  assert (oldSenderBalance#227 == __balance[__msg_sender]);
  }

}

// 
// Function: testSend3 : function (uint256)
procedure testSend3#375(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#290: int)
{
  var oldThisBalance#301: int;
  var oldSenderBalance#308: int;
  var success#314: bool;
  var __send#328: bool;
  assume (__this != __msg_sender);
  oldThisBalance#301 := __balance[__this];
  oldSenderBalance#308 := __balance[__msg_sender];
  success#314 := false;
  if ((__balance[__this] >= amount#290)) {
  call __send#328 := __send(__msg_sender, __this, 0, amount#290);
  success#314 := __send#328;
  }

  if (success#314) {
  assert ((oldThisBalance#301 - amount#290) == __balance[__this]);
  assert ((oldSenderBalance#308 + amount#290) == __balance[__msg_sender]);
  }
  else {
  assert (oldThisBalance#301 == __balance[__this]);
  assert (oldSenderBalance#308 == __balance[__msg_sender]);
  }

}

// 
// Function: testSendError : function (uint256)
procedure testSendError#427(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#377: int)
{
  var oldSum#381: int;
  var __send#403: bool;
  var newSum#407: int;
  oldSum#381 := (__balance[__this] + __balance[__msg_sender]);
  if ((__balance[__this] >= amount#377)) {
  call __send#403 := __send(__msg_sender, __this, 0, amount#377);
  }

  newSum#407 := (__balance[__this] + __balance[__msg_sender]);
  assert (oldSum#381 == (newSum#407 + 1234));
}

// 
// Function: testCall : function (address,uint256) returns (bool)
procedure testCall#442(__this: address_t, __msg_sender: address_t, __msg_value: int, addr#429: address_t, param#431: int)
  returns (#434: bool)
{
  var __call#439: bool;
  call __call#439 := __call(addr#429, __this, 0);
  #434 := __call#439;
  return;
}

