program p;

var
   a : set of 0..10;
   b : set of 20..100;
   s : integer;
   
begin
   b := [20];
   a := [];
   if a = b then
      writeln('bad')
   else
      writeln('good');
end.
