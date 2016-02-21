unit unit_file;

interface
type
   uutype = 5..23;
procedure uufoo(v : uutype) ;

var
   uui : integer;


implementation
procedure uufoo(v : uutype);
begin
   uui := 7 + v;
   
   writeln('Hello from a unit');
end;

begin
   uui := 9;
end.
