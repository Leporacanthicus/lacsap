program p;

type
   c1 = class
     i: integer;
   end;
   pc1 = ^c1;

   c2 = class(c1)
   end;
   pc2 = ^c2;


var
   a : pc1;
   b : pc2;

begin
   new(a);
   new(b);
   b := a;
end.
