program p;

var
   v : array [1..10] of integer;
   w : array [5..12] of integer;
   i : integer;

procedure proc(x: array[l..h: integer] of integer);

var i: integer;
begin
   writeln("In Proc: l=", l, " h=", h);
   for i := l to h do
      writeln(x[i]);
end;

begin
   for i := 1 to 10 do
      v[i] := i;
   for i := 5 to 12 do
      w[i] := 3*i;
   proc(v);
   proc(w);
end.
