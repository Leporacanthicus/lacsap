program main

var a : integer;

{this is a comment}

(* Another comment containing { and  *)

{ A comment containing (* }

function frob(x : integer; y: integer ) : integer;
begin
   frob := x + y;
end;

function foo(x : integer; y: integer ) : integer;
begin
   if x <> 0 then
      foo := frob(x * 2, y * 2)
   else
      foo := frob(x, y) + x + y;
end;

function bar(y :  real) : real;
begin
   y := y * 7.9;
   bar := y * 3.1415926;
end; 


function baz(i : integer): real;
begin
   for i := 0 to 100 do
      bar(1.2);
   baz := 5.0 * (4.0 * 3.9);
end; { baz }


function fact(n	: integer): integer;
begin
   if n < 1 then
   begin
      fact := 1;
   end else begin
      fact := fact(n - 1) * n;
   end;
end; { fact }


function fact2(n : integer): integer;

var b : integer;
    i : integer;
   
begin
   b := 1;
   for i := 1 to n do
   begin
      b := b * i;
   end;
   
   fact2 := b;
end; { fact2 }


procedure testRoundTrunc(r : real);
begin
   writeln('trunc(', r:1:3, ')=', trunc(r));
   writeln('trunc(', -r:1:3, ')=', trunc(-r));
   writeln('round(', r:1:3, ')=', round(r));
   writeln('round(', -r:1:3, ')=', round(-r));
end; { testRoundTrunc }
   
			


begin
   a := 12;
   writeln('Hello, world!');
   write(4711);
   writeln(3.14);
   writeln('a=', a);
   writeln('Factorial of ', 12:1, ' is:', fact(12));

   writeln('fact2(13) = ', fact2(13));
   writeln;
   writeln(foo(2, 3):1);
   writeln('abs(-3.7)=', abs(-3.7));
   testRoundTrunc(2.61);
   testRoundTrunc(6.2);
   readln;
end.
