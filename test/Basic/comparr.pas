program p;

var
   a : array [1..26] of char;
   b : array [1..26] of char;

begin
   a := 'abcdefghijklmnopqrstuvwxyz';
   b := 'abcdefghijklmnopqrstuvwxyz';

   writeln(a = b);

   b[5] := '!';

   writeln(a = b);
end.
