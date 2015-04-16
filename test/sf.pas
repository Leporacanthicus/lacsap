program StaticFieldsForever;
 
type 
  Ts1 = object 
    f1 : longint;
    f2 : longint; static; 
  end; 
 
  Ts2 = object (Ts1)
  end; 
 
var 
  A1, B1 : Ts1; 
  A2, B2 : Ts2; 
 
begin 
  writeln;
 
  A1.f1:=2;
  B1.f1:=3;
  writeln('f1');
  writeln(' A1 :', A1.f1, '  B1 :', B1.f1); 
  writeln(' A2 :', A2.f1, '  B2 :', B2.f1); 
  writeln('-------');
 
  A2.f1:=4; 
  writeln('f1');
  writeln(' A1 :', A1.f1, '  B1 :', B1.f1); 
  writeln(' A2 :', A2.f1, '  B2 :', B2.f1); 
  writeln('-------');
 
  B1.f2:=5; 
  writeln('f1');
  writeln(' A1 :', A1.f1, '  B1 :', B1.f1); 
  writeln(' A2 :', A2.f1, '  B2 :', B2.f1); 
  writeln('f2');
  writeln(' A1 :', A1.f2, '  B1 :', B1.f2, ' T1 :', Ts1.f2); 
  writeln(' A2 :', A2.f2, '  B2 :', B2.f2, ' T2 :', Ts2.f2); 
  writeln('-------');
 
  Ts2.f2:=6; 
  writeln('f2');
  writeln(' A1 :', A1.f2, '  B1 :', B2.f2, ' T1 :', Ts1.f2); 
  writeln(' A2 :', A2.f2, '  B2 :', B2.f2, ' T2 :', Ts2.f2); 
  writeln;
end.
