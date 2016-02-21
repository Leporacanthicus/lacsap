program t3;

var
   x  : integer;
   y  : integer;
   z  : integer;
   da : array [1..10, 1..10] of integer;

begin
   for x := 1 to 10 do
      for y := 1 to 10 do
      begin
	 da[y, x] := z;
	 z := z + 1
      end;
   for x := 1 to 10 do
   begin
      for y := 1 to 10 do
	 write(da[x][y]:2, ' ');
      writeln;
   end;
end.
