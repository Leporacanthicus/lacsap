program d2;

type
   r = record
	  i	       : integer;
	  case Variant : boolean of
	    false : (x: integer;);
	    true  : (y: integer;);
	end;
   q = record
	  rr :  r;
       end;  
       
var
   aa : q;

procedure p(var qq :  q);
begin
   with qq do
   begin
      rr.x := 11;
      rr.y := 23;
   end;
end; { p }

begin
   p(aa);
   with aa do
      writeln(rr.x:3, rr.y:3);
end.
