program calcwords;

const
   NWORDS =  10;

var
   longest : array [1..NWORDS] of string;
   word	   : string;
   i, j	   : integer;
   
procedure readword(var w : string );

begin
   readln(w);
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
   while (i <= len) and ok do
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
   for i := 1 to NWORDS do
      longest[i] := '';
   while not eof do
   begin
      readword(word);
      if check(word) then
      begin
	 if length(word) > length(longest[NWORDS]) then
	 begin
	    i := NWORDS;
	    while (i > 1) and (length(word) > length(longest[i])) do i := i - 1;
	    for j := NWORDS-1 downto i do
	       longest[j+1] := longest[j];
	    longest[i] := word;
	 end;
      end;
   end;
   for i := 1 to NWORDS do
      writeln('Longest word:', longest[i]);
end.
	 
