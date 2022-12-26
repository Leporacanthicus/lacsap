program Complex_Maths(output);

var
   c: Complex;
   d: Complex;

procedure print_complex(s : string; x: complex);
begin
   WriteLn(s:5, ": ", Re(x):8:4, ' + ', Im(x):8:4, 'i');
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
   print_complex("Abs", Abs(c));
   print_complex("Sqr", Sqr(c));
   print_complex("Sqrt", Sqrt(c));
end.
