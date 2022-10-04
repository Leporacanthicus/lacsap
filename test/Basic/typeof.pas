program typeof;

procedure p(a : integer);

var
   b: type of a;
	 
begin
   b := succ(a);
   writeln("b=", b);
end;

begin
   p(2);
end.
