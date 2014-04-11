program cfuncname;

(* Test to ensure we don't have name collisions with C function names *)

(* Use an arbitrary C function name to do something completely different in Pascal *)
function strcpy(x : integer; y : integer) : integer;
begin
   strcpy := x * y
end; { strcpy }

begin
   writeln(strcpy(3,4):1)
end.
