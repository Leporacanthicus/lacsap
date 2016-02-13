program t2;

procedure pp(procedure pp(a, b : integer));

begin
   pp(12, 13);
end; { pp }

procedure qq(a, b : integer);
begin
   writeln(a:4, b:4);
end; { qq }


function indy(a	: integer): integer;
begin
   indy := a div 2;
end;


procedure pf(function f(z : integer): integer);
begin
   writeln(f(18):1);
end;

procedure pcf(procedure pc(function f4(z : integer): integer);
               function f3(n :  integer):integer);
begin
   pc(f3);
end;

begin
   pp(qq);
   pcf(pf, indy);
end.
      
