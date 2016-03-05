program p;

function f(x, y	:  integer): integer;

begin
   f := x + y;
end; { f }

procedure p(function ff(x, y : integer) : integer);
begin
   ff(1);
end;

begin
   p(f);
end.
