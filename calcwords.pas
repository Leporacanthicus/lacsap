program calcwords;

var
   longest : string;
   word	   : string;

procedure readword(var w :string );

begin
   w := '';
   while not eoln do
   begin
      w := w + input^;
      get(input);
   end;
   get(input);
end; { readword }

procedure toupper(var w	: string);
var
   i : integer;
begin
   for i := 1 to length(w) do
   begin
      if w[i] in ['a'..'z'] then
      begin
	 w[i] := chr(ord(w[i]) - 32);
      end;
   end;
end;

(* Check if word is valid in calculator characters *)
function check(var w :  string): boolean;

var
   i   : integer;
   len : integer;
   ok  : boolean;
   
begin
   i := 1;
   len := length(w);
   ok := true;
   toupper(w);
   while (i < len) and ok do
   begin
      if not (w[i] in ['B', 'E', 'G', 'H', 'I', 'L', 'O', 'S', 'Z']) then
      begin
	 ok := false;
      end;
      i := i + 1;
   end;
   check := ok;
end; { check }

begin
   longest := '';
   while not eof do
   begin
      readword(word);
      if check(word) then
      begin
	 if length(word) > length(longest) then
	    longest := word;
      end;
   end;
   writeln('Longest word:', longest);
end.
	 
