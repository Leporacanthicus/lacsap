program i;

procedure p; inline;
begin
   writeln('p');
end; { p }

function f: integer; inline;

begin
   f := 17;
end; { f }

begin
   p;
   writeln('f=', f:3);
end.
   
