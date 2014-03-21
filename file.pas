program filetest;

var
   f : text;
   x : integer;
   
begin
   assign(f, 'test.txt');
   reset(f);
   read(f, x);
   writeln(x);
   close(f);
end.
