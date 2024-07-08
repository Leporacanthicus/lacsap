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

procedure mults;

var
   res : complex;

begin
   res := c * 7;
   print_complex("c * 7", res);
   res := c * 7.5;
   print_complex("c * 7.5", res);
   res := 8.5 * c;
   print_complex("8.5 * c", res);
   res := 8 * c;
   print_complex("8 * c", res);
end;

procedure divs;

var
   res :  complex;

begin
   res := 1 / c;
   print_complex("1 / c", res);
   res := c / 2;
   print_complex("c / 2", res);
   res := c / 0.5;
   print_complex("c / 0.5", res);
end;

procedure exprs;

var
   res : complex;

begin
   res := (c + 7) / 2;
   print_complex("(c + 7) / 2", res);
   res := sqrt(c * c);
   print_complex("sqrt(c*c)", res);
   res := sqrt(sqr(c));
   print_complex("sqrt(sqr(c))", res);
   res := sqrt(sqrt(c * c) * sqrt(c * c));
   print_complex("sqrt(sqrt(c*c)*sqrt(c*c))", res);
   res := cos(c) + sin(c);
   print_complex("cos(c)+sin(c)", res);
end;   

begin
   c := Cmplx(2, 3);
   d := Cmplx(1.5, 2.5);
   print_complex("c", c);
   print_complex("c+d", c+d);
   print_complex("c-d", c-d);
   print_complex("c*d", c*d);
   print_complex("c/d", c/d);
   print_complex("c**3.5", c**3.5);
   print_complex("c POW 3", c POW 3);
   WriteLn;
   print_bool("c=d", c=d);
   print_bool("c=c", c=c);
   print_bool("c!=d", c<>d);
   print_bool("c!=c", c<>c);
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
   WriteLn;
   mults;
   WriteLn;
   divs;
   WriteLn;
   exprs;
end.
