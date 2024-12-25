program we;

type
   day	  = (Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday);
   colour = (Red, Green, Blue, Black, Yellow, Cyan, Magenta);
   
var
   i : day;
   c : colour;
   
begin
   for i := Monday to Sunday do 
      writeln(i);
   writeln;
   writeln(Monday:8, Friday:8, Saturday:9);
   for c := Green to Magenta do
      writeln(c);
end.
