program v;

type
   colour =  (red, green, blue);

var
   i : integer value 42;
   r :  real value 4.2 * 10;

procedure p;

var
   v : colour value blue;

begin
   writeln("v=", ord(v));
end;

begin
   p;
   writeln("i=", i);
   writeln("r=", r);
end.

