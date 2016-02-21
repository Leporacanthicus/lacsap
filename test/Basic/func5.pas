program test;

function a : integer;

var
   x : integer;

function b : integer;

begin
   b := 42;
   x := 83;
end; { b }

begin
   b;
   a := x;
end; { a }

begin
   writeln(a:3);
end.
