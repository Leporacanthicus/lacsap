program heapsort;

type
   arr1000 = array [0..1000] of integer;
   str1000 = array [0..1000] of char;

var
   v   : arr1000;
   n   : integer;
   i   : integer;
   msg : str1000;
	     
procedure sift(n,k:integer;var v:arr1000);
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
    
procedure heapsort(n:integer;var v:arr1000);
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
    
function check(n : integer;  v : arr1000):boolean;
var
    i:integer;
begin
    check:=true;
    for i:=1 to n-1 do
	if(v[i]>v[i+1])then check:=false
end;

procedure diagnose(n : integer; v : arr1000 ;var msg: str1000);
const
    ok = 'Heapsort did it''s job well. All numbers are well sorted';
    notok = 'Heapsort failed. The numbers are not well sorted. It''s your idiotic interpreter''s fault!';
begin
    if not check(n,v) then
       writeln(notok)
    else
       writeln(ok)
end;

begin
    readln(n);
    for i:=1 to n do
	readln(v[i]);

    heapsort(n,v); 
    
    diagnose(n,v,msg);
end.
