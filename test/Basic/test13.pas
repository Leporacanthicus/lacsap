program flow_control;
var
   i : integer;
begin
   i := 1;
   if i < 10 then
      writeln(1:1);
   if i > 10 then
      writeln(2:1);

   if i < 10 then
      writeln(3:1)
   else
      writeln(4:1);

   if i > 10 then
      writeln(5:1)
   else
      writeln(6:1);

   for i := 1 to 5 do
      write(i:1);
   writeln;
   for i := 5 downto 1 do
      write(i:1);

   writeln;
end.
