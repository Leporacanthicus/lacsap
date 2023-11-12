program ss;

var
   s : string;

begin
   s := 'abcdefghijklmnopqrstuvwxyz';
   writeln(substr(s, 14));
   writeln(substr(s, 14, 13));
   writeln(substr(s, 1, 13));
end.
