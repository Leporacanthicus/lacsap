program test;
var i:integer;

procedure beta(x :integer) ; forward;

procedure alfa(x:integer);
var i:integer;
begin
    i:=x+1;
    beta(i)
end;

procedure beta(x:integer);
begin
    i:=x+1
end;

begin
    alfa(3);
   writeln(i:1)
end.
