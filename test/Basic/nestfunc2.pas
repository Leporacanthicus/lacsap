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

procedure c;

var b: integer;

   procedure inner(b : integer);

      procedure inner2;
      begin
	 b:= 7;
      end;

   begin
      inner2;
      writeln(b);
   end;

begin
   b := 2;
   inner(b);
   writeln(b);
end;

begin
   a;
   b;
   c;
end.

