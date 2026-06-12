unit nanotime_pas;

{$mode objfpc}{$H+}

interface

const
  CLOCK_MONOTONIC = 1;

type
  timespec = record
    tv_sec: NativeInt;
    tv_nsec: NativeInt;
  end;

function NanoTime: uint64;

function clock_gettime(clk_id: Integer; var tp: timespec): Integer; cdecl; external 'c' name 'clock_gettime';

implementation

uses
  SysUtils;

//function clock_gettime(clk_id: Integer; var tp: timespec): Integer; cdecl; external 'c' name 'clock_gettime';

function NanoTime: uint64;
var
  ts: timespec;
begin
  clock_gettime(CLOCK_MONOTONIC, ts);
  Result := uint64(ts.tv_sec) * 1000000000 + uint64(ts.tv_nsec);
end;

end.