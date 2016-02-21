program f;

var
   i : record
	  x :integer;
       end;

begin
   for i := 0 to 30 do
      writeln(i:5:2);
end.
