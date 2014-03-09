program typetest;

type
   letter   = 'A'..'Z';
   days	    = (monday, tuesday, wednesday, thursday, friday, saturday, sunday);
   workdays =  monday..friday;

var
   ll	   : letter;
   d	   : days;
   w	   : workdays;
   
function f : integer;
var a	   : array [char] of integer;
begin
   w := wednesday;
   d := saturday;
   f := ord(w) + ord(d);
end;
   
begin
   ll := 'B';
   writeln('LL=', ll);
   for ll := 'K' to 'X' do
      writeln('LL=', ll);
   writeln('f=', f:2);
end.
