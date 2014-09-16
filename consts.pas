program constants;

const
   a  = 42;
   b  = a;
   c  = -42;
   aa = 'a';
   bb = true;
   cc = not bb;
      
begin
   writeln('a=', a:3, ' b=', b:3, ' c=', c:3, ' bb=', bb, ' cc=', cc);
   writeln('aa=', aa:3);
end.
