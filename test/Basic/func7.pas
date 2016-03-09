program p;

type
   tstring =  string[10];


procedure foo(function x (y : Boolean) : tstring);
begin
  writeln (x(true))
end;

function bar(x : boolean):tstring;

begin
   if x then bar := 'OK' else bar := 'failed';
end; { bar }

begin
   foo(bar);
end.
