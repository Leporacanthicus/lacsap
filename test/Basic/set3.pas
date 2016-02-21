program test;

var
   pi1, pi2 : ^integer;

begin
   new(pi1);
   new(pi2);
   pi1^ := 3;
   pi2^ := 5;
   write([pi1^..pi2^] = [3..5]:5);
end.
