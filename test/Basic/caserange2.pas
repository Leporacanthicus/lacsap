program caserange;

var
   i : integer;

begin
   for i := 1 to 15 do
      case i * 1001 of
	1000..1999: writeln("One thousand: ", i * 1001);
	2000..2999: writeln("Two thousand: ", i * 1001);
	3000..3999: writeln("Three thousand: ", i * 1001);
	4000..4999: writeln("Four thousand: ", i * 1001);
	5000..5999: writeln("Five thousand: ", i * 1001);
	6000..6999: writeln("Six thousand: ", i * 1001);
	7000..7999: writeln("Seven thousand: ", i * 1001);
	8000..8999: writeln("Eight thousand: ", i * 1001);
	9000..9999: writeln("Nine thousand: ", i * 1001);
	10000..10999: writeln("Ten thousand: ", i * 1001);
	11000..11999: writeln("Eleven thousand: ", i * 1001);
	12000..12999: writeln("Twelve thousand: ", i * 1001);
	13000..13999: writeln("Thirteen thousand: ", i * 1001);
	14000..14999: writeln("Fourteen thousand: ", i * 1001);
	15000..15999: writeln("Fifteen thousand: ", i * 1001);
	otherwise Writeln("Don't know");
      end;
end.
	
