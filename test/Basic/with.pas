program withtest;

type
   rec = record
	    a : integer;
	    b : record
		   c : integer;
		end; 
	    c : integer;
	 end;	     

var
   v : rec;

begin
   v.a := 2;
   v.b.c := 4;
   v.c := 5;
   with v, b do
      c := 7;
   writeln(v.a, ' ', v.b.c);
end.
