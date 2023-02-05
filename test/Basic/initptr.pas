program p;

type
   pint	=  ^integer;

var
   pp : pint value nil;

begin
   if pp = nil then
      new(pp)
   else
      writeln("Init didn't work");
   
   if pp = nil then
      writeln("Bad stuff happened")
   else
      writeln("All good");
end.
