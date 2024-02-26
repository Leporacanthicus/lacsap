program size;

type
   t = array [1..20] of integer;
   u = record
	  l : longint;
	  r : real;
       end;

const
   a = sizeof(t);
   b = sizeof(u);
   c = sizeof(4);

var
   v : integer;
   w : longint;
   x : real;
   y : boolean;
   z : char;
   f : t;

const
   d = sizeof(z) * 5;
   e = sizeof(f[0]);

begin
   writeln('sizeof(v):    ', sizeof(v));
   writeln('sizeof(w):    ', sizeof(w));
   writeln('sizeof(x):    ', sizeof(x));
   writeln('sizeof(y):    ', sizeof(y));
   writeln('sizeof(z):    ', sizeof(z));

   writeln('sizeof(t):    ', sizeof(t));
   writeln('sizeof(u):    ', sizeof(u));
   writeln('sizeof(f[1]): ', sizeof(f[1]));

   writeln('a=', a);
   writeln('b=', b);
   writeln('c=', c);
   writeln('d=', d);
   writeln('e=', e);
end.
