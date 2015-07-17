program useGoto;

procedure fun(a : integer);

label 99, 1;

begin
 1:
   if a = 0 then
      goto 99;
   a := pred(a);
   writeln('A=', a:1);
   goto 1;
 99:
end; { fun }


begin
   fun(3);
   fun(5);
end.

