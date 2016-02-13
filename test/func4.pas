program test;

function a : integer;

function b : integer;

begin
   a := 42;
   b := 18;
end; { b }

begin
   b;
end; { a }

begin
   writeln(a:3);
end.
