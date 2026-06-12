program test_sum_pas;

{$mode objfpc}{$H+}

uses
  SysUtils, nanotime_pas;


//-----------------------------------------------------------------------

function CalcSum(amax : int64) : int64;
var
  i: int64;
begin
  result := 0;
  for i := 1 to amax do
  begin
    result += i;
  end;
end;

var
  maxval : int64 = 100000000;
  t1, t2 : uint64;
  sum    : int64;
begin
  WriteLn('Sum time test [FPC]');

  if ParamCount > 0 then
  begin
    maxval := StrToint64Def(ParamStr(1), 100000000);
  end;

  WriteLn('Calculating sum 1..', maxval, ' ...');

  t1 := NanoTime();
  sum := CalcSum(maxval);
  t2 := NanoTime();

  WriteLn('sum = ', sum);
  WriteLn('Total exec time: ', t2 - t1, ' ns');
end.
