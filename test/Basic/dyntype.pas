program dyntype;

procedure proc(a, b, c: integer);

var
   x : a..b;

begin
   x := c;
   writeln(x);
end;

begin
   proc(0, 42, 17);
end.
