program q;

type c = class
   i:  integer;
           procedure p;
	 end;

procedure c.p;
begin
   writeln("In p");
end;

procedure c.p;
begin
   writeln("in q");
end;

var
   x : c;

begin
   x.p;
end.
