program time;

var
   ts : timestamp;

begin
   ts.Year := 2004;
   ts.Month := 4;
   ts.Day := 15;
   ts.Hour := 11;
   ts.Minute := 8;
   ts.Second := 10;
   writeln(time(ts));
   writeln(date(ts));
end.
