program p;

var
   a :  integer;

begin
   a := 8;
   for a := 1 to 14 do begin
      write(a, ": ");
      case a of
	1..6: writeln("A bit too small");
	7..8: writeln("About right");
	9..12: writeln("A bit too large");
	otherwise writeln("Way off");
      end;
   end;
end.
