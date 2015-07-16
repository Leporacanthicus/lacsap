program numeric;

{ Test for various numeric constants }

var
   r : real;

begin
   r := -9.99E9;
   writeln(r:14:2);
   r := 0.12345;
   writeln(r:11:9);
   r := 9.99E+9;
   writeln(r:14:2);
   r := 9.99E-9;
   writeln(r:11:9);
   r := 1.23456789123456;
   writeln(r:3:15);
end.
