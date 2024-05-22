program p;

type
   Schema(n: integer) = 0..n;
   Schema2(m, n: integer) = m..n;
   SchArr(n: integer)	  =  array [0..n] of integer;
   SchSet(m, n: integer) = set of m..n;

var
   sc  : Schema(4);
   sc2 : Schema2(-3, 8);
   sca : SchArr(12);
   ss  : SchSet(1, 11);
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
   ss := [1, 3, 7, 9];
   for i in ss do
      write(i:3);
   writeln;
end.
