program populationcount;

begin
   writeln(popcnt(5):2);
   writeln(popcnt(16):2);
   writeln(popcnt(31):2);
   writeln(popcnt(-1):2);

   writeln(popcnt(['a'..'z']):3);
   writeln(popcnt([1..2]):2);
end.
