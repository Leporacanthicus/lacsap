program stringcompare;

begin
   WriteLn(EQ('Hello', 'Hello'));
   WriteLn(LT('Hello', 'Holle'));
   WriteLn(GT('Hello', 'Hallo'));
   WriteLn(NE('Hello', 'Hello'));
   WriteLn(LE('Hello', 'Hello'));
   WriteLn(GE('Hello', 'Hello'));
end.
