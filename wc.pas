program wc;

type
   counts = record
	       chars : integer;
	       words : integer;
	       lines : integer;
	    end;

var
   wc :  counts;

procedure wordcount(var c : counts);
var
   f	  : text;
   ch	  : char;
   inWord : boolean;
begin
   assign(f, 'wc.pas');
   reset(f);
   while not eof(f) do
   begin
      while not eoln(f) do
      begin
	 read(f, ch);
	 c.chars := c.chars + 1;
	 case ch of
	   ' ',
	   '.',
	   ',',
	   '=',
	   '+',
	   '-',
	   '(',
	   ')' : c.words := c.words + 1;
	 end;
      end;
      readln(f);
      if not eof(f) then
      begin
	 c.lines := c.lines + 1;
	 c.chars := c.chars + 1;
      end;
   end;
end;

begin
   wordcount(wc);
   writeln(wc.lines:5, wc.words:5, wc.chars);
end.
