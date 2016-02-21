program p;

const 
   c = 7;
   d = 9;

var
   i : integer;

begin
   readln(i);
   case i of
     c : writeln('C');
     d : writeln('D');
   end; { case }
end.
