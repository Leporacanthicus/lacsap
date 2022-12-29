program Complex_Maths(output);

var
   c: Complex;
   d: Complex;

procedure print_complex(s : string; x: complex);
begin
   WriteLn(s:5, ": ", Re(x):8:4, ' + ', Im(x):8:4, 'i');
end;

procedure print_real(s : string; x: real);
begin
   WriteLn(s:5, ": ", x:8:4);
end;

procedure print_bool(s : string; x: boolean);
begin
   WriteLn(s:5, ": ", x:7);
end;

begin
   c := Cmplx(2, 3);
   d := Cmplx(1.5, 2.5);
   print_complex("c", c);
   print_complex("c+d", c+d);
   print_complex("c-d", c-d);
   print_complex("c*d", c*d);
   print_complex("c/d", c/d);
   WriteLn;
   print_bool("c=d", c=d);
   print_bool("c=c", c=c);
   print_bool("c!=d", c=d);
   print_bool("c!=c", c=c);
   WriteLn;
   print_real("Abs", Abs(c));
   print_Real("Arg", Arg(c));
   WriteLn;
   print_complex("Atan", Arctan(c)); 
   print_complex("Cos", Cos(c));
   print_complex("Exp", Exp(c));
   print_complex("Ln", Ln(c));
   print_complex("Sin", Sin(c)); 
   print_complex("Sqr", Sqr(c));
   print_complex("Sqrt", Sqrt(c));
   print_complex("Tan", Tan(c)); 
   WriteLn;
   print_complex("Polar:", Polar(2, 3));
end.
