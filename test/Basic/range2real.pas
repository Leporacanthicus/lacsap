program range2real;

type
   range = 0..46;

var
   v : range;

procedure p(r : real);

begin
   writeln(r:8:5);
end;

begin
   v := 12;
   p(v + 1.25);
end.
