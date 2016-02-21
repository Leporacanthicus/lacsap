program test;

type
   s =  set of 1..10;

var
   sa : s;
   sb : s;
   i  : integer;

begin
   sa := [1..3];
   sb := [7..9];

   for i := 1 to 10 do
      if i in sa + sb then write(i:2);
   writeln;
end.
