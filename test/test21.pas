program mergesort;

var
    v[1001]:integer;
    n:integer;
    i:integer;
    msg[1001]:char;
    
procedure mergesort(n:integer;var v[1001]:integer);
var 
    i,j,k,m,t:integer;
    left[1001],right[1001]:integer;
begin
    if n>2 then
	begin
	    m:=n div 2;
	    for i:=0 to m-1 do
		left[i]:=v[i];
	    for i:=m to n-1 do
		right[i-m]:=v[i];
			
	    mergesort(m,left);
	    mergesort(n-m,right);
	    i:=0;j:=0;k:=0;
	    while(k<n)do
		begin
		    if((left[i]<=right[j]) or (j>n-m-1)) and (i<m)then
			    begin
				v[k]:=left[i];
				i:=i+1
			    end;
		    else
			    begin
				v[k]:=right[j];
				j:=j+1
			    end;
		    k:=k+1
		end
	end;
    else
	if (n=2) and (v[0]>v[1]) then
	begin
	    t:=v[0];v[0]:=v[1];v[1]:=t
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
    for i:=0 to n-2 do
	if(v[i]>v[i+1])then check:=false
end;

procedure diagnose(n,v[1001]:integer;var msg[1001]:char);
const
    ok[1001]:='Mergesort did it''s job well. All numbers are well sorted';
    notok[1001]:char:='Mergesort failed. The numbers are not well sorted. It''s your idiotic interpreter''s fault!';
begin
    if(not check(n,v))then
	strcpy(msg,notok);
    else
	strcpy(msg,ok)
end;

begin
    readln(n);
    for i:=0 to n-1 do
	readln(v[i]);

    mergesort(n,v); 
    
    diagnose(n,v,msg);
    writeln(msg)
end.
