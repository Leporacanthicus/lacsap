program t1;

const
   a = -1;
   b = 30;

type
   rr	   = a..b;
   settype = set of rr;

var
   tset	: settype;
   i	: integer;

begin
   tset := [a];
   for i := 1 to 15 do
      tset := tset + [i];
   for i := a to b do
      if i in tset then write(i:3);
   writeln;
end.

