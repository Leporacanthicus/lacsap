program p;

type
   initialized = integer value 17;

var
   i : initialized;
   j : integer;
   
begin
   writeln(i);
   i := 7;
   writeln(i);
   j := default(initialized) + 3;
   writeln(j);
   i := default(i);
   writeln(i);
end.
