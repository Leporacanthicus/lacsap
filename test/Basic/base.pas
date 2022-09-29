program base;

var
   x : integer;

begin
   x := 11#a;
   writeln(x);
   x := 12#b;
   writeln(x);
   x := 17#gg;
   writeln(x);
   x := 36#100;
   writeln(x);
   x := 16#100;
   writeln(x);
   x := 16#100000;
   writeln(x);
   x := 16#ffffffff;
   writeln(x);
   x := 2#100;
   writeln(x);
   x := 2#10000000000000000000000000000000;
   writeln(x);
   writeln(16#fffffffff);
end.
