program settest;

var
   s : set of 0..40;
   i : integer;

begin
   s := [1, 3, 9, 17];
   for i := 0 to 40 do
      if i in s then
	 write(i:3);
   writeln;
   s := [1..12];
   for i := 0 to 40 do
      if i in s then
	 write(i:3);
   writeln;
end.
