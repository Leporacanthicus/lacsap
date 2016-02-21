program cmp;
begin
	writeln(2 = 2);
	writeln(2 = 3);
	writeln(true = true);
	writeln(true = false);
        writeln;
	
	writeln(2 <> 2);
	writeln(2 <> 3);
	writeln(true <> true);
	writeln(true <> false);
        writeln;

	writeln(0 < 0);
	writeln(0 < 1);
	writeln(false < false);
	writeln(false < true);
        writeln;
	
	writeln(0 >= 0);
	writeln(0 >= 1);
	writeln(false >= false);
	writeln(false >= true);
        writeln;
	
	writeln(1 <= 0);
	writeln(1 <= 1);
	writeln(true <= false);
	writeln(true <= true);
        writeln;

	writeln(1 > 0);
	writeln(1 > 1);
	writeln(true > false);
        writeln(true > true);
        writeln;

end.
