program wstr;

var
   s : string;
   t : string;

begin
   t := 'Hello';
   writestr(s, 11:4, 7.5:5:2, ' ', true, 'abc':4, t:7);
   writeln(s);
end.
