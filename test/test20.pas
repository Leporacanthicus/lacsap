program heapsort;

var
    v[1001]:integer;
    n:integer;
    i:integer;
    msg[1001]:char;
    
procedure sift(n,k:integer;var v[1001]:integer);
var
    f,t:integer;
begin
    f:=2*k;
    while(f<=n) do
    begin
	if f<n then if v[f+1]>v[f] then f:=f+1;
	if(v[k]>v[f])then k:=f;
	t:=v[k];
	v[k]:=v[f];
	v[f]:=t;
	k:=f;
	f:=2*k
    end
end;
    
procedure heapsort(n:integer;var v[1001]:integer);
var
    i,t:integer;
begin
    for i:=n div 2 downto 1 do
	sift(n,i,v);
	
    while n>0 do
    begin
	t:=v[1];
	v[1]:=v[n];
	v[n]:=t;
	n:=pred(n);
	sift(n,1,v)
    end
end;
    
procedure strcpy(var dest[1001]:char;src[1001]:char);
var
    i:integer;
begin
    i:=0;
    while ord(src[i]) do
    begin
	dest[i]:=src[i];
	i:=succ(i)
    end;
    dest[i]:=chr(0)
end;

function check(n,v[1001]:integer):bool;
var
    i:integer;
begin
    check:=true;
    for i:=1 to n-1 do
	if(v[i]>v[i+1])then check:=false
end;

procedure diagnose(n,v[1001]:integer;var msg[1001]:char);
const
    ok[1001]:='Heapsort did it''s job well. All numbers are well sorted';
    notok[1001]:char:='Heapsort failed. The numbers are not well sorted. It''s your idiotic interpreter''s fault!';
begin
    if(not check(n,v))then
	strcpy(msg,notok);
    else
	strcpy(msg,ok)
end;

begin
    readln(n);
    for i:=1 to n do
	readln(v[i]);

    heapsort(n,v); 
    
    diagnose(n,v,msg);
    writeln(msg)
end.
