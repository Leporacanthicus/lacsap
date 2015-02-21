program sets;


type 
   set1	= set of 1..9;
   set2	= set of 12..46;

var
   s1 : set1;
   s2 : set2;
   i  : integer;

begin
   s1 := [3..7];
   for i := 1 to 9 do
      if i in s1 then
	 write(i:2);
   writeln;
   s2 := [13..45];
   s2 := s2 - [33..38];
   for i := 12 to 46 do
      if i in s2 then
	 write(i:3);
   writeln;
end.

