program minmax;

uses math;

var
   a, b	: integer;
   c, d	: real;
   
begin
   while not eof do
   begin
      readln(a, b);
      writeln('Max=', max(a, b));
      writeln('Min=', min(a, b));
      readln(c, d);
      writeln('Max=', max(c, d):8:3);
      writeln('Min=', min(c, d):8:3);
   end;
end.
