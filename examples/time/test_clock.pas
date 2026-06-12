program test_clock;

const
  CLOCK_MONOTONIC = 1;

type
  timespec = record
    tv_sec: Int64;
    tv_nsec: Int64;
  end;

function clock_gettime(clk_id: Integer; var tp: timespec): Integer; cdecl; external 'c' name 'clock_gettime';

var
  ts: timespec;
begin
  clock_gettime(CLOCK_MONOTONIC, ts);
  WriteLn(ts.tv_sec, ' ', ts.tv_nsec);
end.
