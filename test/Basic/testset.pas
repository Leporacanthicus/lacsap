program testset;

type
   letterset =  set of 'A'..'Z';

var
   a, b	: letterset;
   c	: set of 0..9;
   n	: integer;
   d	: char;

begin
   a := ['A', 'E', 'I'];
   b := [ ];
   c := [ ];

   d := 'B';

   n := 3;

   if c = [ ] then
      writeln('c is an empty set');

   if c + [n mod 10] <= [0..4] then
      c := c + [n mod 10];
   
   if not (d in a) then
      write(d);

   if d in ['A'..'Z'] then
      b := b - [d];
end.
