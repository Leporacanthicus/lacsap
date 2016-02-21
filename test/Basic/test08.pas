program bool_op;
begin
   writeln(0 and 0:0);
   writeln(0 and 2:0);
   writeln(2 and 0:0);
   writeln(1 and 1:0);
   writeln(1 and 2:0);
   writeln;
	
   writeln(0 or 0:0);
   writeln(0 or 2:0);
   writeln(2 or 0:0);
   writeln(1 or 1:0);
   writeln(1 or 2:0);
   writeln;	
	
   writeln(true and false);
   writeln(true and true);
   writeln(false and true);
   writeln(false and false);
   writeln;
	
   writeln(true or false);
   writeln(true or true);
   writeln(false or true);
   writeln(false or false);
   writeln;	
end.
