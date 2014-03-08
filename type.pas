program typetest;

type
   letter = 'A'..'Z';

var
   ll :  letter;
   
begin
   ll := 'B';
   writeln('LL=', ll);
   for ll := 'K' to 'X' do
      writeln('LL=', ll);
end.
