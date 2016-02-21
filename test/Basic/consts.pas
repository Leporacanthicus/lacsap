program constants;

const
   a   = 42;
   b   = a;
   c   = -42;
   d   = -a;
   e   = (7);
   aa  = 'a';
   bb  = true;
   cc  = not bb;
   dd  = a + b;
   sq2 = 1.414213562;
   m   = pi + sq2;
   n   = pi - sq2;
   o   = pi + 1;
   r   = 'World';
   s   = 'Hello, ';
   t   = s + r;
   u   = t + '!';

const
   mul	= 3 * a;
   mul2	= 3 * (a + 1);
   mul3	= 3 * a + 1;
      
begin
   writeln('a=', a:3, ' b=', b:3, ' c=', c:3, ' d=', d:3, ' bb=', bb, ' cc=', cc);
   writeln('aa=', aa:3, ' dd=',dd:3, ' m=', m:11:9, ' n=', n:11:9, ' o=', o:11:9);
   writeln('t=', t, ' u=', u);
   writeln('mul=', mul:3, ' mul2=', mul2:3, ' mul3=', mul3:3);
   writeln('e=', e:3);
end.
