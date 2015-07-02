program sgn;

var
   i : integer;
   a : integer;
   b : real;

begin
   writeln('sign(1)        =', sign(1));
   writeln('sign(0)        =', sign(0));
   writeln('sign(3)        =', sign(3));
   writeln('sign(380000000)=', sign(380000000));
   writeln('sign(-2)       =', sign(-2));
   writeln('sign(-2.0)     =', sign(-2.0));
   writeln('sign(20.00)    =', sign(20.0));
   writeln('sign(2000.0)   =', sign(2000.0));
   writeln('sign(-2E-19)   =', sign(-2E-19));
   writeln('sign(2E18)     =', sign(2E18));

   for i := 1 to 3 do
   begin
      readln(a);
      writeln('sign(a)=', sign(a));
      readln(b);
      writeln('sign(b)=', sign(b));
   end;
end.
   
