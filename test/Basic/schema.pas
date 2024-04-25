program p;

type
   Schema(n: integer) = 0..n;
   Schema2(m, n: integer) = m..n;

var
   sc  : Schema(4);
   sc2 : Schema2(-3, 8);
begin
   sc := 3;
   sc2 := 7;
   writeln(sc, ' ', sc2);
end.
