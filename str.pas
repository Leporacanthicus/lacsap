program str;

var 
   str1	: string;
   str2	: string;
   str3	: string;
   str4	: string;
   str5	: string;

begin
   str1 := 'abc';
   str2 := 'def';
   str3 := 'abcdef';

   str5 := str3 + 'ghi';

   str4 := str1 + str2;

   if str1 + str2 = str3 then
      writeln('same');
   writeln(str1);
   writeln(str2);
   writeln(str4);
   writeln(str5);
end.
