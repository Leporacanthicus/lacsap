program filetest;

var
	f: text;

begin
   assign(f, 'test.txt');
   reset(f);
   close(f);
end.
