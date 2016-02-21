program nestedfunctions;

function func1(x : integer) : integer;

var
   y : integer;
   z : real;
   
  function func2(x : integer) : integer;

    procedure func3;

    begin
       z := 7.2;
    end;

  begin
     y := 3;
     func2 := x * 2;
     func3;
  end; { func2 }

begin
   y := 2;
   func1 := func2(x);
   writeln('x =', x:0, ' y=', y:0, 'z=', z:2:1);
end; { func1 }


begin
   writeln('func1(4) = ', func1(4));
end.
