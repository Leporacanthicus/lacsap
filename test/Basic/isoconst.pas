program iso;

CONST
   linefeed = chr(10); {ASCII}
   bufflen = 200;
   
TYPE
   buffinx     = 0..bufflen-1;
   buffer      = ARRAY [buffinx] OF char;
   DaysOfWeeek =  (Sunday, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday);

CONST
   tabchar = chr(9); {ASCII}
   heading = 'Name' + tabchar + 'Address';
   weekdays = [Monday..Friday];

TYPE
   rainbow = (violet, indigo, blue, green, yellow, orange, red);

   
CONST
   firstcol = violet;
   lastcol = red;
   letters =  ['a'..'z', 'A'..'Z'];

var
   col	       : rainbow;
   i	       : buffinx;
   buf	       : buffer;
   letterCount : integer;
begin
   for col := firstcol to lastcol do
      writeln(ord(col));

   buf := "The quick brown fox jumps over the lazy dog";

   for i := 0 to bufflen-1 do
      if buf[i] in letters then
	 inc(letterCount);
   writeln(letterCount);
end.
