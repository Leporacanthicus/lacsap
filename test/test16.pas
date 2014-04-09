program test_case;
var 
    a:integer;
    b:char;
begin
        a := 3;
        case a of
                1, 2: writeln(1:1);
		3: writeln(3:1);
        end;
	case a of
		1: writeln(3:1);
		2: writeln(2:1);
		otherwise: writeln(1:1)
	end;
        b := '3';
        case b of
                '1': writeln(1:1);
		'2', '3': writeln(3:1);
		'4': writeln(4:1);
        end;
	case b of
		'1': writeln(3:1);
		'2': writeln(2:1);
		otherwise: writeln(1:1)
	end
end.
