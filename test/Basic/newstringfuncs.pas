program newstringfuncs;

var
   a : array [1..10] of char;

begin
   a := '  World   ';
   WriteLn(Index('Hello', 'lo'));
   WriteLn('"', Substr('Hello', 4, 2), '"');
   WriteLn('"', Trim(' Hello '), '"');
   WriteLn('"', Trim(a), '"');
   Writeln('"', Trim('        '), '"');
end.
