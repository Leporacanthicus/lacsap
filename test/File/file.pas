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
   assign(f, 'test1.txt');
   rewrite(f);
   writeln(f, x);
   close(f);
   append(f);
   writeln(f, x * 2);
   close(f);
end.
