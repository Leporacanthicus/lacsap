program rec

var
   fcalls : integer;
   x	  : integer;
   res	  : integer;
	   

function g(x : integer) : integer; forward;

function f(x : integer) : integer;

begin
   fcalls := fcalls + 1;
   if x < 1 then
      f := 1
   else
      f := f(x-1) + g(x);
end;


function g(x : integer) : integer;

begin
   if (x < 2) then
      g := 1
   else
      g := f(x-1) + g(x/2);
end; { g }

begin
   write('Enter a number:');
   readln(x);
   res := f(x);
   writeln('Calls to f:', fcalls:0, ' res=', res:0);
end.
