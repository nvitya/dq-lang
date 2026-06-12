program test_sum_pas;

{$mode objfpc}{$H+}

uses
  SysUtils;

const
  CLOCK_MONOTONIC = 1;

type
  timespec = record
    tv_sec: NativeInt;
    tv_nsec: NativeInt;
  end;

function clock_gettime(clk_id: Integer; var tp: timespec): Integer; cdecl; external 'c' name 'clock_gettime';

function NanoTime: UInt64;
var
  ts: timespec;
begin
  clock_gettime(CLOCK_MONOTONIC, ts);
  Result := UInt64(ts.tv_sec) * 1000000000 + UInt64(ts.tv_nsec);
end;

//-----------------------------------------------------------------------

function CalcSum(amax : Int64) : Int64;
var
  i: Int64;
begin
  result := 0;
  for i := 1 to amax do
  begin
    result += i;
  end;
end;

var
  maxval : Int64 = 100000000;
  t1, t2 : UInt64;
  sum    : Int64;
begin
  WriteLn('Sum time test [FPC]');

  if ParamCount > 0 then
  begin
    maxval := StrToInt64Def(ParamStr(1), 100000000);
  end;

  WriteLn('Calculating sum 1..', maxval, ' ...');

  t1 := NanoTime();
  sum := CalcSum(maxval);
  t2 := NanoTime();

  WriteLn('sum = ', sum);
  WriteLn('Total exec time: ', t2 - t1, ' ns');
end.
