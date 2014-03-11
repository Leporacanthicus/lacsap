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
begin
   w := wednesday;
   d := saturday;
   f := ord(w) + ord(d);
end; 

procedure ptrtest;
var
   p : ^integer;
begin
   new(p);
   p^ := 12;
   dispose(p);
end;


procedure testAccess;

type
   at = array [1..20] of integer;
   
var
   x	: integer;
   p	: ^integer;
   arr	: at;
   parr	: ^at;
   arrp	: array [1..20] of ^integer;

begin
   x := 7;
   new(p);
   p^ := 8;
   arr[x] := 9;
   new(parr);
   parr^[x] := 10;
   new(arrp[x]);
   arrp[x]^ := 11;
   writeln('p^=', p^:0);
   writeln('parr^[x]=', parr^[x]:0);
   writeln('arrp[x]^=', arrp[x]^:0);
end;
   
begin
   ll := 'B';
   writeln('LL=', ll);
   for ll := 'K' to 'X' do
      writeln('LL=', ll);
   writeln('f=', f:2);
   ptrtest;
   testAccess;
end.
