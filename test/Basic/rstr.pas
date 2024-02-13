program p;

var
   s : string;
   i : integer;
   r : real;
   t : string;
   e : 1..80;

begin
   s := '6 8.25 42 abc';
   readstr(s, i, r, e, t);
   writeln('s:', s);
   writeln('i:', i:8);
   writeln('r:', r:8:4);
   writeln('e:', e:8);
   writeln('t:', t:8);
end.
