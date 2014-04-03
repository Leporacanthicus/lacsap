program fact;

const
    base = 10;

type
   NumArray =  array [0..400] of integer;

var
   i   : integer;
   num : NumArray;
    
procedure mulsc(var v : NumArray; n:integer);
var
    i,t:integer;
begin
    for i:=1 to v[0] do
	v[i]:=v[i]*n;
	
    t:=0;
    for i:=1 to v[0] do
    begin
	t:=t+v[i];
	v[i]:=t mod base;
	t:=t div base
    end;

    while t <> 0 do
    begin
	v[0]:=v[0]+1;
	v[v[0]]:=t mod base;
	t:=t div base
    end
end;


procedure print(v : NumArray);
var
    i:integer;
begin
    for i:=v[0] downto 1 do
       write(v[i]:1);
    writeln
end;

procedure fact(var v : NumArray;n:integer);
var 
    i:integer;
begin
    v[0]:=1;
    v[1]:=1;
    for i:=2 to n do
       mulsc(v,i)
end;

begin
    for i:=1 to 200 do
    begin
        fact(num,i);
	print(num)
    end
end.
