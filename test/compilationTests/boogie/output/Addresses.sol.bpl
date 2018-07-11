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
// Function: thisBalance : function (uint256) view returns (bool)
procedure thisBalance#14(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#3: int)
  returns (#6: bool)
{
  #6 := (__balance[__this] > amount#3);
  goto $14end;
  $14end:
}

// 
// Function: testTransfer : function (uint256)
procedure testTransfer#63(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#16: int)
{
  var oldSum#20: int;
  var newSum#46: int;
  oldSum#20 := (__balance[__this] + __balance[__msg_sender]);
  if ((__balance[__this] >= amount#16)) {
  call __transfer(__msg_sender, __this, 0, amount#16);
  }

  newSum#46 := (__balance[__this] + __balance[__msg_sender]);
  assert (oldSum#20 == newSum#46);
  $63end:
}

// 
// Function: testTransfer2 : function (uint256)
procedure testTransfer2#126(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#65: int)
{
  var oldThisBalance#76: int;
  var oldSenderBalance#83: int;
  assume (__this != __msg_sender);
  oldThisBalance#76 := __balance[__this];
  oldSenderBalance#83 := __balance[__msg_sender];
  if ((__balance[__this] >= amount#65)) {
  call __transfer(__msg_sender, __this, 0, amount#65);
  assert ((oldThisBalance#76 - amount#65) == __balance[__this]);
  assert ((oldSenderBalance#83 + amount#65) == __balance[__msg_sender]);
  }

  $126end:
}

// 
// Function: testTransferError : function (uint256)
procedure testTransferError#178(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#128: int)
{
  var oldSum#132: int;
  var newSum#158: int;
  oldSum#132 := (__balance[__this] + __balance[__msg_sender]);
  if ((__balance[__this] >= amount#128)) {
  call __transfer(__msg_sender, __this, 0, amount#128);
  }

  newSum#158 := (__balance[__this] + __balance[__msg_sender]);
  assert (oldSum#132 == (newSum#158 + 1234));
  $178end:
}

// 
// Function: testSend : function (uint256)
procedure testSend#227(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#180: int)
{
  var oldSum#184: int;
  var __send#206: bool;
  var newSum#210: int;
  oldSum#184 := (__balance[__this] + __balance[__msg_sender]);
  if ((__balance[__this] >= amount#180)) {
  call __send#206 := __send(__msg_sender, __this, 0, amount#180);
  }

  newSum#210 := (__balance[__this] + __balance[__msg_sender]);
  assert (oldSum#184 == newSum#210);
  $227end:
}

// 
// Function: testSend2 : function (uint256)
procedure testSend2#301(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#229: int)
{
  var oldThisBalance#233: int;
  var oldSenderBalance#240: int;
  var success#246: bool;
  var __send#260: bool;
  oldThisBalance#233 := __balance[__this];
  oldSenderBalance#240 := __balance[__msg_sender];
  success#246 := false;
  if ((__balance[__this] >= amount#229)) {
  call __send#260 := __send(__msg_sender, __this, 0, amount#229);
  success#246 := __send#260;
  }

  if (success#246) {
  assert ((oldThisBalance#233 + oldSenderBalance#240) == (__balance[__this] + __balance[__msg_sender]));
  }
  else {
  assert (oldThisBalance#233 == __balance[__this]);
  assert (oldSenderBalance#240 == __balance[__msg_sender]);
  }

  $301end:
}

// 
// Function: testSend3 : function (uint256)
procedure testSend3#388(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#303: int)
{
  var oldThisBalance#314: int;
  var oldSenderBalance#321: int;
  var success#327: bool;
  var __send#341: bool;
  assume (__this != __msg_sender);
  oldThisBalance#314 := __balance[__this];
  oldSenderBalance#321 := __balance[__msg_sender];
  success#327 := false;
  if ((__balance[__this] >= amount#303)) {
  call __send#341 := __send(__msg_sender, __this, 0, amount#303);
  success#327 := __send#341;
  }

  if (success#327) {
  assert ((oldThisBalance#314 - amount#303) == __balance[__this]);
  assert ((oldSenderBalance#321 + amount#303) == __balance[__msg_sender]);
  }
  else {
  assert (oldThisBalance#314 == __balance[__this]);
  assert (oldSenderBalance#321 == __balance[__msg_sender]);
  }

  $388end:
}

// 
// Function: testSendError : function (uint256)
procedure testSendError#440(__this: address_t, __msg_sender: address_t, __msg_value: int, amount#390: int)
{
  var oldSum#394: int;
  var __send#416: bool;
  var newSum#420: int;
  oldSum#394 := (__balance[__this] + __balance[__msg_sender]);
  if ((__balance[__this] >= amount#390)) {
  call __send#416 := __send(__msg_sender, __this, 0, amount#390);
  }

  newSum#420 := (__balance[__this] + __balance[__msg_sender]);
  assert (oldSum#394 == (newSum#420 + 1234));
  $440end:
}

// 
// Function: testCall : function (address,uint256) returns (bool)
procedure testCall#455(__this: address_t, __msg_sender: address_t, __msg_value: int, addr#442: address_t, param#444: int)
  returns (#447: bool)
{
  var __call#452: bool;
  call __call#452 := __call(addr#442, __this, 0);
  #447 := __call#452;
  goto $455end;
  $455end:
}

// 
// Function: literals : function () pure
const unique address_0xf17f52151EbEF6C7334FAD080c5704D77216b732: address_t;
const unique address_0x627306090abaB3A6e1400e9345bC60c78a8BEf57: address_t;
procedure literals#483(__this: address_t, __msg_sender: address_t, __msg_value: int)
{
  var a1#459: address_t;
  var a2#463: address_t;
  var a3#467: address_t;
  a1#459 := address_0xf17f52151EbEF6C7334FAD080c5704D77216b732;
  a2#463 := address_0x627306090abaB3A6e1400e9345bC60c78a8BEf57;
  a3#467 := address_0xf17f52151EbEF6C7334FAD080c5704D77216b732;
  assert (a1#459 == a3#467);
  assert (a1#459 != a2#463);
  $483end:
}

