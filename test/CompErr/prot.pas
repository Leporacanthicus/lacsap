program p;

procedure proc(protected a : integer);

begin
   a := 7;
end;

begin
   proc(6);
end.
