program test_dynarr_pas;

{$mode objfpc}{$H+}
{$pointermath on}

uses
    SysUtils, nanotime_pas;

const
    default_maxval: int64 = 1000000;

var
    darr: array of int32;

procedure FillArray(maxval: int32);
var
    i : int32;
    len, cap : int32;
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

procedure FillArrayPtr(maxval: int32);
var
    i    : int32;
    pi32 : ^int32;
begin
    SetLength(darr, maxval);
    pi32 := @darr[0];
    for i := 0 to maxval - 1 do
    begin
        pi32[i] := i;
    end;
end;

function CalcSum : int64;
var
    i, arrlen : int32;
begin
    result := 0;
    arrlen := Length(darr);
    for i := 0 to arrlen - 1 do
    begin
        result += darr[i];
    end;
end;

function CalcSumPtr : int64;
var
    i, arrlen : int32;
    pi32      : ^int32;
begin
    result := 0;
    arrlen := Length(darr);
    pi32   := @darr[0];
    for i := 0 to arrlen - 1 do
    begin
        result += pi32[i];
    end;
end;

var
    maxval : int64;
    t1, t2 : uint64;
    sum    : int64;
begin
    WriteLn('DynArray Test [FPC]');

    maxval := default_maxval;
    if ParamCount > 0 then
    begin
        maxval := StrToint64Def(ParamStr(1), default_maxval);
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
