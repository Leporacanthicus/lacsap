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

procedure test(a : r; var b :r);
begin
   b := a;
end; { test }

begin
   c.x := 4;
   c.y := 9.2;
   c.z := true;
   test(c, d);
   writeln('x=', d.x:1, ' y=', c.y:3:1, ' z=',c.z);
end.
