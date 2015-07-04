program str;

var 
   str1	: string;
   str2	: string;
   str3	: string;
   str4	: string;
   str5	: string;
   str6	: string;
   str7	: string;
   str8	: string;
   str9	: string[30];
   ch	: char;

procedure proc(var str : string);
begin
   str := 'doremifasole';
end;

begin
   str1 := 'abc';
   str2 := 'def';
   str3 := 'abcdef';

   str5 := str3 + 'ghi';

   str4 := str1 + str2;

   str6 := 'a';

   str6 := str6 + 'b';

   str6 := 't' + str6;

   if str1 + str2 = str3 then
      writeln('same');

   str7 := '';
   for ch := 'A' to 'Z' do
      str7 := str7 + ch;

   str8 := copy(str7, 2, 20);

   proc(str9);

   writeln(str1);
   writeln(str2);
   writeln(str4);
   writeln(str5);
   writeln(str6);
   writeln(str7);
   writeln(str8);
   writeln(length(str8));
   writeln(str9);
end.
