program typetest;

type
   letter   = 'A'..'Z';
   days	    = (monday, tuesday, wednesday, thursday, friday, saturday, sunday);
   workdays =  monday..friday;

var
   ll : letter;
   
function f : integer;
var a :  array [char] of integer;
begin
end;
   
begin
   ll := 'B';
   writeln('LL=', ll);
   for ll := 'K' to 'X' do
      writeln('LL=', ll);
end.
