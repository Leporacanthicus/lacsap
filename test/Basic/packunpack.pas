program packunpack;

type
   arri	= array (.1..10.) of integer;

var
   avi	: arri;
   pavi	: packed array [1..10] of integer;
   i	: integer;
   x	: integer;
   ci	: char;
   cia	: array [char] of integer;
   pcia	: packed array [char] of integer;
   
begin
   for i := 1 to 10 do
      avi[i] := i+20;
   pack(avi, 1, pavi);

   for i := 10 downto 1 do
      write(pavi[i]:1, ' ');
   writeln;

   for i := 1 to 10 do
      pavi[i] := i+30;
   unpack(pavi, cia, 'g');
   for ci := 'p' downto 'g' do
      write(cia[ci]:1, ' ');
   writeln;

   x := 1;
   for ci := 'a' to 'z' do
   begin
      cia[ci] := x;
      x := x+1
   end;
   pack(cia, 'm', pavi);
   for i := 10 downto 1 do
      write(pavi[i]:1, ' ');
   writeln;
end.   
