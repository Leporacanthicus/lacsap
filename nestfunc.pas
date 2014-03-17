program nestedfunctions;

function func1(x : integer) : integer;

  function func2(x : integer) : integer;
  begin
     func2 := x * 2;
  end; { func2 }

begin
   func1 := func2(x);
end; { func1 }


begin
   writeln('func1(4) = ', func1(4));
end.
