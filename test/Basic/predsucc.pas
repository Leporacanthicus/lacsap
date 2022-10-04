program predsucc;


type
   colour =  (white, red, green, blue, purple, black);

var
   a : integer;
   b : colour;

begin
   a := 3;
   a := succ(a, 2);
   writeln("a=", a);
   a := pred(a, 3);
   writeln("a=", a);

   b := red;
   writeln("b=", ord(b));
   b := succ(b, 3);
   writeln("b=", ord(b));
   b := pred(b, 4);
   writeln("b=", ord(b));
   if b <> white then
      writeln("This is wrong, b should be white")
   else
      writeln("Correct, b is white");
end.   
