program builtin_func;
var a:integer; b:char;
begin
   a := 3;
   b := 'a';
   writeln(succ(a):1);
   writeln(succ(b):1);

   a := 66;
   b := 'b';
   writeln(pred(a):1);
   writeln(pred(b):1);
   writeln(ord(b):1);
   writeln(chr(a):1)
end.
