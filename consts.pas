program constants;

const
   a   = 42;
   b   = a;
   c   = -42;
   d   = -a;
   aa  = 'a';
   bb  = true;
   cc  = not bb;
   dd  = a + b;
   pi  = 3.1415926;
   sq2 = 1.414213562;
   m   = pi + sq2;
   n   = pi - sq2;
      
begin
   writeln('a=', a:3, ' b=', b:3, ' c=', c:3, ' d=', d:3, ' bb=', bb, ' cc=', cc);
   writeln('aa=', aa:3, ' dd=',dd:3, ' m=', m:11:9, ' n=', n:11:9);
end.
