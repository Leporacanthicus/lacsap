program typetest;

const
   a = 1;
   b = 'b';
   c = 'A string';
   d = 3.9;
   e = true;

type
   letter   = 'A'..'Z';
   days	    = (monday, tuesday, wednesday, thursday, friday, saturday, sunday);
   workdays = monday..friday; 
   ptrInt   = ^integer;
   ptrMonth = ^month;
   month    = (january, february, march, april, may, june, july, august, september,
	       october, november, december);
   range    = 8..16;

   rec = record
	    f1 : integer;
	    f2 : real;
	 end;  
	    
var   
   ll : letter;
   dd : days;
   w  : workdays;
   rr : range;
   
function f : integer;
var
   b :  integer;
begin 
   w  := wednesday;
   dd := saturday;
   f  := ord(w) + ord(dd);
   b  := 7;
end;  

procedure ptrtest;
var
   p : ^integer;
begin
   new(p);
   p^ := a;
   writeln('p^=', p^:0);
   dispose(p);
end; { ptrtest }

procedure recordtest;
var
   r : rec;
   s : record
	  g1 : integer;
	  g2 : integer;
	  g3 : longint;
       end;  
begin
   r.f1 := 2;
   r.f2 := d;
   s.g2 := 8;
   s.g1 := 9;
   s.g3 := 12345678909112;
   writeln('r.f1=', r.f1:0);
   writeln('r.f2=', r.f2:0);
   writeln('s.g1=', s.g1:0);
   writeln('s.g2=', s.g2:0);
   writeln('s.g3=', s.g3:0);
end;
   

procedure testAccess;

const
   h =  'Another string';

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
   writeln('h=', h);
end; { testAccess }

procedure proc(c : char; d: letter);
begin
   writeln('Proc: c=', c, ' d=', d);
end;

   
begin
   ll := 'B';
   proc(ll, 'C');
   writeln('LL=', ll);
   for ll := 'K' to 'X' do
      writeln('LL=', ll);
   writeln('f=', f:2);
   ptrtest;
   testAccess;
   recordtest;
   rr := 8;
   rr := rr + 1;
   rr := 1 + rr;
   writeln('RR=', rr:1);
end.
