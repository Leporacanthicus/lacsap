program longcompile;

type
   r =  record
	   a :  array [1..8000] of integer;
	end;

var
   b : r;
   i : integer;

procedure p1(x : r);
var y : r;
begin
   y := x;
   writeln(y.a[2]);
end; { p1 }

procedure p2(x :  r);
var y : r;
begin
   y := x;
   p1(y);
end; { p2 }

procedure p3(x :  r);
var y : r;
begin
   y := x;
   p2(y);
end; { p3 }

begin
   for i := 1 to 8000 do
      b.a[i] := i;
   p3(b);
end.
