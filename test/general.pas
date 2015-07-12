program main;

var a : integer;
    b : real;
   
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

procedure ifemptyelse;

begin
   if a < 0 then
   else writeln('a is greater than zero');
end;

function testIntToFloat(x :  integer) : real;
var r :  real;
begin
   r := x;
   testIntToFloat := r;
end; { testIntToFloat }
   
procedure testRoundTrunc(r : real);
begin
   writeln('trunc(', r:1:3, ')=', trunc(r));
   writeln('trunc(', -r:1:3, ')=', trunc(-r));
   writeln('round(', r:1:3, ')=', round(r));
   writeln('round(', -r:1:3, ')=', round(-r));
end; { testRoundTrunc }
   
procedure testProcWithNoArgs;
begin
   writeln('In procedure with no args');
end; { testProcWithNoArgs }

procedure testChar;
var
   ch : char;
   i  : integer;
   
begin
   ch := 'a';
   for i := 1 to 10 do
   begin
      write(ch);
      ch := succ(ch);
   end;
   for i := 1 to 10 do
   begin
      write(ch);
      ch := pred(ch);
   end;
   writeln;
end; { testChar }

procedure readTest;
var
   x : integer;
   y : real;

begin
   write('Enter a number:');
   readln(x);
   writeln('x=', x:2);
   write('Enter a floating point number:');
   readln(y);
   writeln('y=', y:2:8);
end; { readTest }


procedure updateVars(var x : integer; var y: real);
begin
   x := 42;
   y := 17.243;
end; { updateVars }

procedure testUpdateVars;
var x : integer; y : real;
begin
   updateVars(x, y);
   writeln('x=',x:1,' y=',y:8:3);
end; { testUpdateVars }

procedure whileTest;
var x :  integer;
begin
   x := 10;
   while x > 0 do
   begin
      writeln('x=', x:1);
      x := x-1;
   end;
end; { whileTest }

procedure repeatTest;
var y : integer;
begin
   y := 0;
   repeat
      writeln('y=', y:1);
      y := y + 1;
   until y > 10;
end; { repeatTest }

procedure arrayTest;
var
   arr : array [1..100] of integer;
   i   : integer;
begin
   for i := 1 to 100 do
      arr[i] := i;

   for i := 1 to 100 do
      writeln('arr[', i:0, ']=', arr[i]:0);
end; { arrayTest }
   
begin
   a := 12;
   ifemptyelse;
   writeln('Hello, world!');
   write(4711);
   writeln(3.14:5:3);
   writeln('a=', a);
   writeln('Factorial of ', 12:1, ' is:', fact(12));
   writeln('fact2(13) = ', fact2(13));
   writeln;
   writeln(foo(2, 3):1);
   writeln('abs(-3.7)=', abs(-3.7):8:4);
   testRoundTrunc(2.61);
   testRoundTrunc(6.2);
   testProcWithNoArgs;
   writeln('testIntToFloat(12)=', testIntToFloat(12):5:3);

   writeln('sqr(16)=', sqr(16));
   writeln('sqr(4.2)=', sqr(4.2):8:3);
   writeln('sqrt(16)=', sqrt(16):8:3);
   writeln('sqrt(169.0)=', sqrt(169.0):8:3);

   whileTest;
   repeatTest;
   arrayTest;
   
   testUpdateVars;
   testChar;
   write('Enter a number:');
   readln(a);
   writeln('a=', a:2);
   write('Enter a floating point number:');
   readln(b);
   writeln('a=', b:2:5);
   readTest;
end.
