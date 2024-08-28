program randomizetest;

type
   range = 1..20;
   arr	 = array [range] of integer;

var
   i : integer;
   a : arr;
   b : arr;
   c : arr;
   d : arr;
   e : arr;

procedure getrandarray(var a : arr);

var
   i: integer;

begin
   for i := 1 to 20 do
      a[i] := random(maxint);
end;

function comparr(a : arr; b: arr) : boolean;

var
   i: integer;

begin
   comparr := true;
   for i := 1 to 20 do
      if a[i] <> b[i] then
	  comparr := false;
end;

begin
   getrandarray(a);
   randomize;
   getrandarray(b);
   if not comparr(a, b) then
      writeln('Good, a and b should not be the same');
   randomize(4711);
   getrandarray(c);
   if not (comparr(a, c) or comparr(b, c)) then
      Writeln('All good, neither a and c or b and c should be the same');
   randomize(1832);
   getrandarray(d);
   if not comparr(c, d) then
      writeln('Cool, c and d should also be different');
   randomize(4711);
   getrandarray(e);
   if comparr(c, e) then
      writeln('Great, c and e are equal, as they should be');
end.
