program val;

type
   t  = integer value 42;
   r  = real value 19.3;
   c  = char value '*';
   e  = (green, blue, red) value blue;
   aa = array [1..20] of real;
   bb = aa[1..10: 7.125 otherwise 9.5];
   cc = array [1..10] of aa;
   dd = array [1..3] of char value '*';

const
   init	 = aa[1..20: 2.375];
   init2 = aa[1..5: 2.5 otherwise 5];
   init3 = cc[1..4: init2 otherwise init];
   
var
   n  : t;
   m  : r;
   x  : r value 18.7;
   a  : c;
   b  : c value '#';
   v  : e;
   w  : e value red;
   xx : bb;
   yy : dd;
   
begin
   writeln("n=", n, " m=", m, " x=", x);
   writeln("a=", a, " b=", b);
   writeln("v=", ord(v), " w=", ord(w));
   if v <> w then writeln("Good");
   writeln("xx[1]=", xx[1]:8:4, " xx[10]=", xx[10]:8:4, " xx[11]=", xx[11]:8:4, " xx[19]=", xx[19]:8:4,
	   " xx[20]=", xx[20]:8:4);
   writeln("init3[1][1]=", init3[1][1]:8:4," init3[2][2]=", init3[2][2]:8:4," init3[4][6]=", init3[4][6]:8:4,
	   " init3[5][1]=", init3[5][1]:8:4," init3[10][17]=", init3[10][17]:8:4);
   writeln("yy=", yy);
end.
