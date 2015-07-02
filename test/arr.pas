program arr;

type
   e = (Mon, Tue, Wed, Thu, Fri, Sat, Sun);
   a =  array [Mon..Sun] of integer;

var
   aa		: a;

begin
   aa[Mon] := 7;
   writeln('aa[Mon]=', aa[Mon]);
end.
