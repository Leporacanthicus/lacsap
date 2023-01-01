program initrec;

type
   rec	    = record
		 x	: integer;
		 y : real;
	      end; 
   initRec  = rec value [x:3; y:6.3];
   initRec2 =  initRec value [x:7; y:19.2];
   rec2 = record
	     x : integer value 5;
	     y : real value 42.7;
	  end;

var
   a : initRec;
   b : rec;
   c : initRec2;
   d : rec2;
   
begin
   writeln("a:", a.x, a.y:8:4);
   b := a;
   b.x := b.x + 1;
   b.y := b.y + 0.5;
   writeln("b:", b.x, b.y:8:4);
   writeln("c:", c.x, c.y:8:4);
   c := b;
   writeln("c:", c.x, c.y:8:4);
   writeln("d:", d.x, d.y:8:4);
end.
