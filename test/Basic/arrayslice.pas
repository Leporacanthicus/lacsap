program as;

var
   a : array [1..5] of char;

procedure proc(x : array [l..h : integer] of char);

var
   i :  integer;
begin
   for i := l to h do
      write(x[i]);
   writeln;
end;

begin
   a := "abcde";
   proc(a);
   proc(a[2..4]);
   proc(a[1..2]);
end.
