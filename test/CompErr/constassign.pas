program p;

type
   r =  record
	   x: integer;
	   y: integer;
	end;

const
   c = r[x:2; y:3];
   d = 4;

begin
   c.x := 4;
end.
