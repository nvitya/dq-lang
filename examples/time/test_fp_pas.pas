program test_fp_pas;

{$mode objfpc}{$H+}

uses
    SysUtils, nanotime_pas;

const
    default_f1: Double = 2.200002;
    default_f2: Double = 2.200001;
    default_million_iter: Integer = 5;

function MilliTime : Double;
var
    ts: timespec;
begin
    clock_gettime(CLOCK_MONOTONIC, ts);
    Result := Double(ts.tv_sec) * 1000.0 + Double(ts.tv_nsec) / 1000000.0;
end;

function FpBench_f32(f1, f2: Single; iterations: Integer) : Single;
var
    i: Integer;
begin
    result := 1.0;
    for i := 0 to iterations - 1 do
    begin
        result := result * f1;
        result := result / f2;
    end;
end;

function FpBench_f64(f1, f2: Double; iterations: Integer) : Double;
var
    i: Integer;
begin
    result := 1.0;
    for i := 0 to iterations - 1 do
    begin
        result := result * f1;
        result := result / f2;
    end;
end;

var
    f1, f2, ans: Double;
    million_iter, iterations: Integer;
    tstart, tend: Double;
begin
    WriteLn('Floating Point Benchmark [FPC]');

    f1 := default_f1;
    f2 := default_f2;
    million_iter := default_million_iter;

    if ParamCount > 0 then
    begin
        million_iter := StrToIntDef(ParamStr(1), default_million_iter);
    end;

    iterations := million_iter * 1000000;

    WriteLn('FP32 benchmark with F1=', f1:0:6, ', F2=', f2:0:6, ', million_iterations=', million_iter, ':');
    tstart := MilliTime();
    ans := FpBench_f32(f1, f2, iterations);
    tend := MilliTime();
    WriteLn('  ans = ', ans:0:6, ', time = ', (tend - tstart):0:3, ' ms');
    WriteLn('  ', Trunc(iterations / (tend - tstart)), ' loop/msec');

    WriteLn('FP64 benchmark with F1=', f1:0:6, ', F2=', f2:0:6, ', million_iterations=', million_iter, ':');
    tstart := MilliTime();
    ans := FpBench_f64(f1, f2, iterations);
    tend := MilliTime();
    WriteLn('  ans = ', ans:0:6, ', time = ', (tend - tstart):0:3, ' ms');
    WriteLn('  ', Trunc(iterations / (tend - tstart)), ' loop/msec');
end.
