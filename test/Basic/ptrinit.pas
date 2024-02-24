program pi;

type
   ptr =  ^integer value nil;

var
   p :  ptr;

begin
   if p = nil then
      writeln('Is NIL')
   else
      writeln('Not NIL');
end.
