program recpass;

(* Record passing, assignment of records, etc *)

type
r = record
       x : integer;
       y : real;
       z : boolean;
    end;

var
   c : r;
   d : r;
   e : r;

procedure test(a : r; var b :r);
begin
   b := a;
end; { test }

function test2(a : r) : r;
begin
   test2 := a;
end;

begin
   c.x := 4;
   c.y := 9.2;
   c.z := true;
   test(c, d);
   e := test2(d);
   writeln('x=', e.x:1, ' y=', e.y:3:1, ' z=',e.z);
end.
