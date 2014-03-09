program typetest;

type
   letter   = 'A'..'Z';
   days	    = (monday, tuesday, wednesday, thursday, friday, saturday, sunday);
   workdays = monday..friday; 
   ptrInt   = ^integer;
   ptrMonth = ^month;
   month    = (january, february, march, april, may, june, july, august, september,
	       october, november, december);
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
end; { f }

procedure ptrtest;
var
   p : ^integer;
begin
   new(p);
   dispose(p);
end;
   
begin
   ll := 'B';
   writeln('LL=', ll);
   for ll := 'K' to 'X' do
      writeln('LL=', ll);
   writeln('f=', f:2);
   ptrtest;
end.
