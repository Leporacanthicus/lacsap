program p;

var 
   a : string[20];
   n : integer;
begin
   a := 'abcdefghij';

   for n := 1 to 10 do
      writeln(a[1..n]);
end.
