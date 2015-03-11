program param;

var
   i   : integer;
   str : string;

begin
   for i := 0 to ParamCount do begin
      str := paramstr(i);
      writeln("Param ", i:1, "='", str, "'");
   end;
end.
