program boolops;

var
   b :  boolean;

function bb(s : string) : boolean;

begin
   writeln(s);
   bb := true;
end;
   
begin
   b := true;
   if b and_then bb("This should print") then
      writeln("We should get here");
   if b or_else not bb("This should not print") then
      writeln("We should get here");
   if not b or_else not bb("This should print") then
      writeln("We should not get here");
end.
