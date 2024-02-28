program val;

type
   t = integer value 42;
   r = real value 19.3;
   c = char value '*';
   e = (green, blue, red) value blue;
   aa = array [1..20] of real;
   bb = aa[1..20: 7.125];
   
var
   n : t;
   m : r;
   x : r value 18.7;
   a : c;
   b : c value '#';
   v : e;
   w : e value red;
   xx : bb;
   
begin
   writeln("n=", n, " m=", m, " x=", x);
   writeln("a=", a, " b=", b);
   writeln("v=", ord(v), " w=", ord(w));
   if v <> w then writeln("Good");
   writeln("xx[1]=", xx[1]:8:4, " xx[11]=", xx[11]:8:4, " xx[19]=", xx[19]:8:4,
	   " xx[20]=", xx[20]:8:4);
end.
