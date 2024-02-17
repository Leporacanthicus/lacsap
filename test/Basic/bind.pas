program b;

var
   m : bindingtype;
   t : text;

begin
   m := binding(t);
   m.Name := "myfile.txt";
   Bind(t, m);
   if Binding(t).Bound then
      writeln("success")
   else
      writeln("Failed");
end.
