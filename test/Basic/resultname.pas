program x;

function f(x : integer) = g: integer;

begin
   if x > 1 then g := f(x-1) * x
   else g := 1;
end;

begin
   writeln(f(8));
end.
   
