program test_sl;
var
   i : integer;

procedure proc_1;
var
   j : integer;

   procedure proc_2;
   begin
      writeln(i + 2);
      writeln(j + 2)
   end; 

   procedure proc_2a;
   begin
      i := 110;
      j := 220;
      proc_2
   end;

begin
   j := 200;
   writeln(i + 1);
   writeln(j + 1);
   proc_2;
   proc_2a
end;

begin
   i := 100;
   proc_1
end.
