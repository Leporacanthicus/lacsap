program arr4;
var
   a : array [1..10] of array ['a'..'j'] of integer;
   b : array ['a'..'j', 1..10] of integer;
   i : integer;
   j : char;
   
begin
   for i := 1 to 10 do begin
      for j := 'a' to 'j' do begin
	 a[i][j] := i * ord(j);
	 b[j][i] := (i+1) * ord(j);
      end;
   end;
   writeln(a[6, 'b']);
   writeln(b['b', 6]);
end.
