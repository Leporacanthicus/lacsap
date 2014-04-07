program test_case;
var 
    a:integer;
    b:char;
begin
        a := 3;
        case a of
                1, 2: writeln(1);
		3: writeln(3);
        end;
	case a of
		1: writeln(3);
		2: writeln(2);
		otherwise: writeln(1)
	end;
        b := '3';
        case b of
                '1': writeln(1);
		'2', '3': writeln(3);
		'4': writeln(4);
        end;
	case b of
		'1': writeln(3);
		'2': writeln(2);
		otherwise: writeln(1)
	end
end.
