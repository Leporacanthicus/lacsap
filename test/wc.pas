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
   inWord := false;
   while not eof(f) do
   begin
      while not eoln(f) do
      begin
	 read(f, ch);
	 c.chars := c.chars + 1;
	 if ((ch >= 'a') and (ch <= 'z')) or ((ch >='A') and (ch <= 'z')) then
	 begin
	    inWord := true;
	 end;
	 case ch of
	   ' ':
	 begin
	    if inWord then c.words := c.words + 1;
	    inWord := false;
	 end;
	   
	 end;
      end;
      readln(f);
      if not eof(f) then
      begin
	 c.lines := c.lines + 1;
	 if inWord then
	 begin
	    c.words := c.words + 1;
	 end;
	 c.chars := c.chars + 1;
      end;
   end;
end;

begin
   wordcount(wc);
   writeln(' ', wc.lines:1, ' ', wc.words:1, ' ', wc.chars:1);
end.
