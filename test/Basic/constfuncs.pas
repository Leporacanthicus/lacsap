program cf;

type
   weekdays =  (Sunday, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday);

const
   a = chr(65);
   b = 41;
   c = succ(b);
   d = pred(pred(b));
   e = ord(a);
   f = sin(4.7);
   h = cos(4.7);
   g = length('abcdef');
   i = succ(b, 2);
   j = pred(b, 5);
   l = ln(0.7);
   k = exp(0.7);
   m = ord(Monday);
   n = succ(Monday);
   o = pred(n, 2);
   p = 12/7;
   q = 12 div 7;
   r = 14 mod 3;
   s = 11 and 5;
   t = 11 or 5;
   u = 11 xor 5;
   v = 11 shl 3;
   w = 4711 shr 4;
   
begin
   writeln('a=', a);
   writeln('b=', b);
   writeln('c=', c);
   writeln('d=', d);
   writeln('e=', e);
   writeln('f=', f:15:13);
   writeln('h=', h:15:13);
   writeln('g=', g);
   writeln('i=', i);
   writeln('j=', j);
   writeln('l=', l:15:13);
   writeln('k=', k:15:13);
   writeln('m=', m);
   writeln('n=', ord(n));
   writeln('o=', ord(o));
   writeln('p=', p:10:8);
   writeln('q=', q);
   writeln('r=', r);
   writeln('s=', s);
   writeln('t=', t);
   writeln('u=', u);
   writeln('v=', v);
   writeln('w=', w);
end.
