program v;

type
   colour =  (red, green, blue);

var
   i : integer value 42;
   r :  real value 4.2 * 10;

procedure p;

var
   v : colour value blue;
   s : set of char value ['a'..'d', 'x', 'A'];
   c : char;

begin
   writeln("v=", ord(v));
   for c in s do
      write(c, " ");
   writeln;
end;

begin
   p;
   writeln("i=", i);
   writeln("r=", r);
end.

