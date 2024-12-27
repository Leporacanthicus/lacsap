program xx;

type
   r = record
	  x : integer;
	  y : real;
       end; 

a = array [1..3] of r;

const
   c =  a[1: [x:1; y:2.5], 2: [x:2; y:3.0], 3: [x:3; y:3.5] ];

var
   i : r;


begin
   for i in c do
      writeln(i.x, i.y);
end.
   
