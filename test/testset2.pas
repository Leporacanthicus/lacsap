program testset2;
Type  
  Day = (mon,tue,wed,thu,fri,sat,sun);  
  Days = set of Day;  
 
Procedure PrintDays(W : Days);  

Var  
   D	 : Day;
   first : boolean;
   
begin
   first := true;
   write('[');
   For D:=mon to sun do
   begin
      if D in W then  
      begin  
	 if not first then
	    write(',');
	 first := false;
	 case D of
	   mon : write('mon');
	   tue : write('tue');
	   wed : write('wed');
	   thu : write('thu');
	   fri : write('fri');
	   sat : write('sat');
	   sun : write('sun');
	 end; { case }
      end;
   end;
   Writeln(']');  
end;  
 
Var  
  W : Days;  
 
begin  
   W:=[mon,tue]+[wed,thu,fri]; {// equals [mon,tue,wed,thu,fri]  }
   PrintDays(W);
   W:=[mon,tue,wed]-[wed];     {// equals [mon,tue]  }
   PrintDays(W);  
   W:=[mon,tue,wed]-[wed,thu];     {// also equals [mon,tue]  }
   PrintDays(W);  
   W:=[mon,tue,wed]*[wed,thu,fri]; {// equals [wed]  }
   PrintDays(W);
(*   W:=[mon,tue,wed]><[wed,thu,fri]; {// equals [mon,tue,thu,fri]  } 
   PrintDays(W);  *)
end.
