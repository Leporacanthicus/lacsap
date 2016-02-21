program q;

var
   a	      : array [1..10] of array [1..10] of array [1..10] of array [1..10] of integer;
   b	      : array [1..10, 1..10, 1..10, 1..10] of integer;
   i, j, k, l : integer;
   
begin
   for i := 1 to 10 do
      for j := 1 to 10 do
	 for k := 1 to 10 do
	    for l := 1 to 10 do
	    begin
	       a[i][j][k][l] := i * j * k * l;
	       b[i][j][k][l] := (i+1) * j * k * l;
	    end;
      
   writeln(a[6, 2][3, 4]);
   writeln(b[6, 2, 3, 4]);
end.
