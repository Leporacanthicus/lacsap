program testcases;


const
   nine	= 1+8;
   five	= 5;

var
   i : integer;

procedure numbertotext(n : integer);

begin
   case n of
     0: writeln('zero');
     0+1: writeln('one');
     1+1: writeln('two');
     1+1*2: writeln('three');
     2+2: writeln('four');
     10/2: writeln('five');
     five+1: writeln('six');
     7+0: writeln('seven');
     0+8: writeln('eight');
     nine: writeln('nine');
     otherwise   writeln('unknown');
   end;
end; { numbertotext }


begin
   for i := -1 to 11 do
      numbertotext(i);
end.   
