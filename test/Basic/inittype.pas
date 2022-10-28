program val;

type
   t = integer value 42;
   r = real value 19.3;
   c = char value '*';
   e = (green, blue, red) value blue;
var
   n : t;
   m : r;
   x : r value 18.7;
   a : c;
   b : c value '#';
   v : e;
   w : e value red;
begin
   writeln("n=", n, " m=", m, " x=", x);
   writeln("a=", a, " b=", b);
   writeln("v=", ord(v), " w=", ord(w));
   if v <> w then writeln("Good");
end.
