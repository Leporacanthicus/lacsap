program p;

procedure a;

var
   x : integer;

   procedure inner;
   begin
      x := x + 1;
   end;

begin
   x := 3;
   inner;
   writeln(x);
end;

procedure b;

var
   y : integer;

   procedure inner;
   begin
      y := y - 2;
   end;

begin
   y := 7;
   inner;
   writeln(y);
end;

begin
   a;
   b;
end.

