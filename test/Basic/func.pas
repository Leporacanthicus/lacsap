program func;

(* Test for function passing to a function *)

const eps = 1E-7;

var
   result : real;
   good	  : boolean;

procedure interval(function f(x	: real): real; a, b, epsvalue: real;
		   var zeropoint : real; var ok : boolean);
var
   m  : real;
   fa : real;

begin
   fa := f(a);
   ok := (fa * f(b) < 0) and (a < b);
   if ok then
   begin
      repeat
	 m := (a + b) / 2;
	 if fa * f(m) > 0 then
	    a := m
	 else
	    b := m;
      until b - a < epsvalue;
      result := m;
   end;
end; { f }


function f(x : real) : real;
begin
   f := 2*x*x*x - x*x - 0.9;
end; { f }

function g(x : real) : real;
begin
   g := f(x) - cos(x) + exp(-x);
end; { g }

begin
   interval(f, 0.5, 1.5, eps, result, good);
   if good then
      writeln('f is zero at', result:12:7)
   else
      writeln('No zero for f found');

   interval(g, 0.5, 1.5, eps, result, good);
   if good then
      writeln('g is zero at', result:12:7)
   else
      writeln('No zero for f found');
end.
