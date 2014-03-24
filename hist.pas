program histogram;

const NumLines = 50;

type
   Letter = 'A'..'Z';
   Vector = array [Letter] of integer;

var
   numLetters : Vector;
   numSpaces  : integer;
   ltrPerAst  : real;


procedure ReadText(var num : Vector; var spaces : integer);
var
   ch : char;
   k  : Letter;

begin
   for k := 'A' to 'Z' do
      num[k] := 0;
   spaces := 0;

   while not eof do
   begin
      while not eoln do
      begin
	 read(ch);
	 if (ch >= 'a') and (ch <= 'z') then
	    ch := chr(ord(ch)-32);
	 if (ch >= 'A') and (ch <= 'Z') then
	    num[ch] := num[ch] + 1
	 else if ch = ' ' then
	    spaces := spaces + 1;
      end;
      readln;
   end;
end;

function MaxCount(var num : Vector) : integer;
var
   max : integer;
   k   : Letter;

begin
   max := -1;
   for k := 'A' to 'Z' do
      if num[k] > max then
	 max := num[k];
   MaxCount := max;
end; { MaxCount }

procedure AdjustCount(var num : Vector; var quote : real);
var
   max : integer;
   k   : Letter;

begin
   max := MaxCount(num);
   if max < NumLines then
      quote := 1.0
   else
   begin
      quote := max/NumLines;
      for k := 'A' to 'Z' do
	 num[k] := round(num[k]/quote);
   end;
end; { AdjustCount }

procedure WriteHistogram(var num : Vector);
var
   row : array [Letter] of char;
   i   : integer;
   k   : Letter;

begin
   for k := 'A' to 'Z' do
      row[k] := ' ';
   for i := NumLines downto 1 do
   begin
      for k := 'A' to 'Z' do
	 if num[k] >= i then
	    row[k] := '*';
      for k := 'A' to 'Z' do
	 write(row[k]:2);
      writeln;
   end;
   for k := 'A' to 'Z' do
      write(' -');
   writeln;
   for k := 'A' to 'Z' do
      write(k:2);
   writeln;
end; { writeHistogram }

begin
   ReadText(numLetters, numSpaces);
   AdjustCount(numLetters, ltrPerAst);
   WriteHistogram(numLetters);
   writeln;
   writeln('Every * corresponds to ', ltrPerAst:6:2, ' letters');
   writeln('There are also ', numSpaces:1, ' spaces');
end.
