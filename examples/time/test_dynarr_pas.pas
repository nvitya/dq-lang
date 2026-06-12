program test_dynarr_pas;

{$mode objfpc}{$H+}
{$pointermath on}

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

//-----------------------------------------------------------------------------------

const
  default_maxval: Int64 = 1000000;

var
  darr: array of Int32;

procedure FillArray(maxval: Int32);
var
  i: Int32;
  len, cap: Int32;
begin
  SetLength(darr, 0);
  len := 0;
  cap := 0;
  for i := 0 to maxval - 1 do
  begin
    if len >= cap then
    begin
      if cap = 0 then cap := 1 else cap := cap * 2;
      SetLength(darr, cap);
    end;
    darr[len] := i;
    Inc(len);
  end;
  SetLength(darr, len);
end;

procedure FillArrayPtr(maxval: Int32);
var
  i: Int32;
  pi32: PInt32;
begin
  SetLength(darr, maxval);
  if maxval > 0 then
  begin
    pi32 := @darr[0];
    for i := 0 to maxval - 1 do
    begin
      pi32[i] := i;
    end;
  end;
end;

function CalcSum: Int64;
var
  i, arrlen: Int32;
  res: Int64;
begin
  res := 0;
  arrlen := Length(darr);
  for i := 0 to arrlen - 1 do
  begin
    res := res + darr[i];
  end;
  Result := res;
end;

function CalcSumPtr: Int64;
var
  i, arrlen: Int32;
  res: Int64;
  pi32: PInt32;
begin
  res := 0;
  arrlen := Length(darr);
  if arrlen > 0 then
  begin
    pi32 := @darr[0];
    for i := 0 to arrlen - 1 do
    begin
      res := res + pi32[i];
    end;
  end;
  Result := res;
end;

var
  maxval: Int64;
  t1, t2: UInt64;
  sum: Int64;
begin
  WriteLn('DynArray Test [FPC]');

  maxval := default_maxval;
  if ParamCount > 0 then
  begin
    maxval := StrToInt64Def(ParamStr(1), default_maxval);
  end;

  WriteLn('maxval = ', maxval);

  WriteLn('Filling the dynamic array...');
  t1 := NanoTime();
  FillArray(maxval);
  t2 := NanoTime();
  WriteLn('Total fill time: ', (t2 - t1) div 1000, ' us');

  WriteLn('Summing the dynamic array...');
  t1 := NanoTime();
  sum := CalcSum();
  t2 := NanoTime();
  WriteLn('sum = ', sum);
  WriteLn('Total sum time: ', (t2 - t1) div 1000, ' us');

  WriteLn;
  WriteLn('Using pointer operations');
  WriteLn;

  WriteLn('Filling the dynamic array (ptr)...');
  t1 := NanoTime();
  FillArrayPtr(maxval);
  t2 := NanoTime();
  WriteLn('Total fill time: ', (t2 - t1) div 1000, ' us');

  WriteLn('Summing the dynamic array (ptr)...');
  t1 := NanoTime();
  sum := CalcSumPtr();
  t2 := NanoTime();
  WriteLn('sum = ', sum);
  WriteLn('Total sum time: ', (t2 - t1) div 1000, ' us');
end.
