program forinset;

type
   smallRange = 1..20;	
   bigRange   = 1000..1500;

var
   c: char;
   b: integer;
   s1: set of char;
   s2: set of smallRange;
   s3: set of bigRange;

begin
   s1 := ['a', 'b', 'c', 'w', 'x', 'y', 'z'];
   for c in s1 do
      writeln(c);

   s2 := [1, 7, 14];
   for b in s2 do
      writeln(b);
   
   s3 := [1000..1005];
   for b in s3 do
      writeln(b);
end.
