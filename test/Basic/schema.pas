program p;

type
   Schema(n: integer) = 0..n;
   Schema2(m, n: integer) = m..n;
   SchArr(n: integer)	  =  array [0..n] of integer;

var
   sc  : Schema(4);
   sc2 : Schema2(-3, 8);
   sca : SchArr(12);
   i   : integer;

begin
   for i := 0 to 12 do
      sca[i] := i;
   sc := 3;
   sc2 := 7;
   writeln(sc, ' ', sc2);
   writeln(sca[1], ' ', sca[5], ' ', sca[12]);
   for i := 0 to 12 do
      sca[i] := i;
end.
