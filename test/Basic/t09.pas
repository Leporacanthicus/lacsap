{ FLAG --field-widths=10 }

{ Borrowed from the Gnu Pascal Tests }

program sideffect(output);
var
        a, z: integer;
function sneaky(x: integer): integer;
        begin
                z := z-x;
                sneaky := sqr(x);
        end;
begin
        z := 10;
        a := sneaky(z);
        writeln(a:10, z:10);
        z := 10;
        a := sneaky(10) * sneaky(z);
        writeln(a:10, z:10);
        z := 10;
        a := sneaky(z) * sneaky(10);
        writeln(a:10, z:10);
end.
