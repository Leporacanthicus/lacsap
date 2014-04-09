program mergesort;

type
   arr1000 = array [0..1000] of integer;
   
var
   v	     : arr1000;
   n	     : integer;
   i	     : integer;
    
procedure mergesort(n : integer; var v: arr1000);
var 
   i,j,k,m,t  : integer;
   left,right : arr1000;
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
	 end
	 else
	 begin
	    v[k]:=right[j];
	    j:=j+1
	 end;
	 k:=k+1
      end
   end
   else
      if (n=2) and (v[0]>v[1]) then
      begin
	 t:=v[0];v[0]:=v[1];v[1]:=t
      end
end;
   
function check(n :integer; v : arr1000):boolean;
var
    i:integer;
begin
    check:=true;
    for i:=0 to n-2 do
	if(v[i]>v[i+1])then check:=false
end;

procedure diagnose(n : integer;  v : arr1000);
const
    ok = 'Mergesort did it''s job well. All numbers are well sorted';
    notok = 'Mergesort failed. The numbers are not well sorted. It''s your idiotic interpreter''s fault!';
begin
    if not check(n,v) then
	writeln(notok)
    else
	writeln(ok)
end;

begin
    readln(n);
    for i:=0 to n-1 do
	readln(v[i]);

    mergesort(n,v); 
    
    diagnose(n,v);
end.
