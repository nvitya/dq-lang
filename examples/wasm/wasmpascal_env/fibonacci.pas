library fibonacci;

// Classic recursive Fibonacci — the standard "does the compiler do
// recursion?" test, plus an iterative loop for comparison. Pure console.

function FibRec(n: Integer): Integer;
begin
  // Ternary expression (Delphi 13 style): `if cond then a else b` used where
  // an expression is expected — here the function result. Only the taken
  // branch runs, so this recurses exactly like the if/else form.
  FibRec := if n <= 1 then n else FibRec(n - 1) + FibRec(n - 2);
end;

function FibLoop(n: Integer): Integer;
var
  a, b, t, i: Integer;
begin
  if n <= 1 then
    FibLoop := n
  else
  begin
    a := 0;
    b := 1;
    for i := 2 to n do
    begin
      t := a + b;
      a := b;
      b := t;
    end;
    FibLoop := b;
  end;
end;

procedure RunDemo;
var
  i: Integer;
begin
  writeln('=== Fibonacci ===');
  writeln(' n    recursive    loop');
  for i := 0 to 15 do
    writeln(i:2, '     ', FibRec(i):8, '     ', FibLoop(i):3);
  writeln;
  writeln('Fib(20) = ', FibRec(20), ' (recursive)');
  writeln('Fib(20) = ', FibLoop(20), ' (loop)');
end;

begin
  RunDemo;
end.
