program cfuncname;

(* Test to ensure we don't have name collisions with C function names *)

var x : integer;

(* Use an arbitrary C function name to do something completely different in Pascal *)
function strcpy(var x : integer; y : integer) : integer;
begin
   if x <> 3 then
      writeln('Huh?');
   strcpy := x * y
end; { strcpy }

begin
   x := 3;
   writeln(strcpy(x,4):1)
end.
