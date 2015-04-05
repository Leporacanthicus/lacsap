program obj;

type
   myobj = object
     i:	 integer;
     s:	 string;
     procedure p;
     procedure q;
     function f(x :integer) : integer;
   end;	     
   myderived = object(myobj)
     j: integer;
   end;

   derived2 = object(myderived)
     k: integer;
     procedure r; static;
   end;

var
   m : myobj;
   n : myderived;
   o : derived2;

procedure myobj.p;
begin
   self.i := 89;
   writeln('Member: p');
end; { myobj }

procedure myobj.q;
begin
   i := 34;
   writeln('Member: q');
end; { myobj }

function myobj.f(x : integer) : integer;
begin
   f := i * x;
end;

procedure derived2.r;
begin
   writeln('Member: r');
end;

begin
   m.i := 42;
   m.s := 'foo';

   writeln('m.i:', m.i);
   m.p;
   writeln('m.i:', m.i);
   m.q;

   writeln('m.i:', m.i);
   writeln('m.s:', m.s);

   writeln('m.f:', m.f(2));

   n.i := 46;
   n.s := "Blah";
   n.j := 92;
   n.p;
   writeln("n.i:", n.i);
   writeln("n.j:", n.j);
   writeln("n.s:", n.s);

   o.i := 18;
   o.r;
   writeln("o.i", o.i);
end.
