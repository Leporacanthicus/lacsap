program val;

type
   t = integer value 42;
   r =  integer value 17;
var
   n : t;
   m : r;
   x : r value 18;

begin
   writeln("n=", n, " m=", m, " x=", x);
end.
