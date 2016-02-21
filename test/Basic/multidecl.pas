program multidecl;

{ Test that multiple declarations inside a procedure works }

procedure test;

const
   n =  10;

type
   arr =  array [0..n] of integer;

var
   a : arr;

const
   n2 = 12;

type
   arr2	=  array [0..n2] of integer;

var
   b : arr2;
   i : integer;

begin
   for i := 0 to n do
      a[i] := i * 2;
   for i := 0 to n2 do
      b[i] := i;

   writeln(b[5]:1);
   writeln(a[4]:1);
end; { test }

begin
   test;
end.
