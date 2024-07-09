program a;

var
   a   : boolean;
   str : string;

begin
   readln(a);
   writeln(a);
   writestr(str, not a);
   readstr(str, a);
   writeln(a);
   readln(a);
   writeln(a);
   writestr(str, not a);
   readstr(str, a);
   writeln(a);
end.
