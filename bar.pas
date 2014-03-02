PROGRAM main

{this is a comment}

(* Another comment containing { and  *)

{ A comment containing (* }

FUNCTION frob(x : integer; y: integer ) : integer;
begin
   frob := x + y;
end;

FUNCTION foo(x : integer; y: integer ) : integer;
BEGIN
   if x <> 0 then
      foo := frob(x * 2, y * 2)
   else
      foo := frob(x, y) + x + y;
END;

function bar(y :  real) : real;
beGin
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
      fact := 1
   else
      fact := fact(n - 1) * n;
end;


begin
   writeln('Hello, world!');
   write(4711);
   write(3.14);
   writeln;
   writeln('Factorial of ', 12:1, ' is:', fact(12));
   writeln;
   writeln(foo(2, 3):1);
end.
