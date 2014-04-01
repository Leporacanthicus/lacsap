program testcases;

type
   days	= (monday, tuesday, wednesday, thursday, friday, saturday, sunday);

var
   i : integer;
   d :  days;

procedure numbertotext(n : integer);

begin
   case n of
     0 :  writeln('zero');
     1 :  writeln('one');
     2 :  writeln('two');
     3 :  writeln('three');
     4 :  writeln('four');
     5 :  writeln('five');
     6 :  writeln('six');
     7 :  writeln('seven');
     8 :  writeln('eight');
     9 :  writeln('nine');
   end;
end; { numbertotext }

procedure weekdays(w : days);
begin
   case w of
     monday    : writeln('Monday');
     tuesday   : writeln('Tuesday');
     wednesday : writeln('Wednesday');
     thursday  : writeln('Thursday');
     friday    : writeln('Friday');
     saturday  : writeln('Saturday');
     sunday    : writeln('Sunday');
   end; { case }
end;

begin
   for i := 0 to 9 do
      numbertotext(i);
   for d := monday to sunday do
      weekdays(d);
end.   
