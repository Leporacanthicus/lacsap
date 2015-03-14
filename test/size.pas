program size;

type
   t = array [1..20] of integer;
   u = record
	  l : longint;
	  r : real;
       end;

var
   v : integer;
   w : longint;
   x : real;
   y : boolean;
   z : char;

begin
   writeln('sizeof(v): ', sizeof(v));
   writeln('sizeof(w): ', sizeof(w));
   writeln('sizeof(x): ', sizeof(x));
   writeln('sizeof(y): ', sizeof(y));
   writeln('sizeof(z): ', sizeof(z));

   writeln('sizeof(t): ', sizeof(t));
   writeln('sizeof(u): ', sizeof(u));
end.
