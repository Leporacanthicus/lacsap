program o;

type
   c1 = class
     i: integer;
     procedure p; virtual;
   end;
   pc1 = ^c1;

   c2 = class(c1)
    procedure p; override;
   end;
   pc2 = ^c2;

procedure c1.p;
begin
   writeln('P: i=', i);
end;

procedure c2.p;
begin
   writeln('P2: i=', i);
end;

function create(i : integer) : pc1;

var
   p1 : pc1;
   p2 : pc2;
   
begin
   case i mod 2 of
     0 : begin
	   new(p1);
	   create := p1;
	 end;
     1 : begin
	    new(p2);
	    create := p2;
	 end;
   end;
end;

var
   a, b	: pc1;
   i	: integer;

begin
   i := 1;
   a := create(i);
   a^.i := 14;
   b := create(i*2);
   b^.i := 18;
   a^.p;
   b^.p;
end.
