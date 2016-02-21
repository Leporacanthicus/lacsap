program loop;

var
   a : array [char] of integer;
   i : char;

begin
   for i := chr(0) to chr(255) do
      a[i] := ord(i);
   for i := chr(0) to chr(255) do
      writeln(a[i]);
end.
   
