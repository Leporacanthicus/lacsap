program k;

const
   s1 = '123456789012';
   s2 = '12345';
   s3 = '3.1415926';

var
   v : longint;
   i : integer;
   r : real;

begin
   Val(s1, v);
   Val(s2, i);
   Val(s3, r);

   writeln('V=', v, ' i=', i, ' r=', r:9:7);
end.
