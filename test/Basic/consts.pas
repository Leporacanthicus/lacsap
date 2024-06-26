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

const
   v = 2;
   w = 3;
   x = v pow w;
   y = 3.5;
   z = y pow w;
   ff = y pow -w;
   gg = -y pow w;
   hh = 2.5;
   ii = v ** hh;
   jj = v ** hh;
   kk = v ** -hh;
   ll = y ** hh;
      
begin
   writeln('a=', a:3, ' b=', b:3, ' c=', c:3, ' d=', d:3, ' bb=', bb, ' cc=', cc);
   writeln('aa=', aa:3, ' dd=',dd:3, ' m=', m:11:9, ' n=', n:11:9, ' o=', o:11:9);
   writeln('t=', t, ' u=', u);
   writeln('mul=', mul:3, ' mul2=', mul2:3, ' mul3=', mul3:3);
   writeln('e=', e:3);
   
   writeln('x=', x);
   writeln('z=', z:10:5);
   writeln('ff=', ff:10:5);
   writeln('gg=', gg:10:5);
   writeln('ii=', ii:10:5);
   writeln('jj=', jj:10:5);
   writeln('kk=', kk:10:5);
   writeln('ll=', ll:10:5);
end.
