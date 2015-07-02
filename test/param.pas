program param;

var
   i   : integer;

begin
   for i := 0 to ParamCount do begin
      writeln('Param ', i:1, '="', paramstr(i), '"');
   end;
end.
