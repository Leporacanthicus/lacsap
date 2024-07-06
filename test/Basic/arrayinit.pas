program arrayinit;

type
   Elements = (H, He, Li, Be, B, C, N, O, F);
   ElemNameType = ARRAY [Elements] OF ARRAY [1..10] OF CHAR;

const
   ElemNames = ElemNameType [ H: 'Hydrogen', He: 'Helium', Li: 'Lithium',
			     Be: 'Beryllium', B: 'Boron', C:'Carbon',
			     N: 'Nitrogen', O: 'Oxygen', F:'Fluorine' ];

var
   e : elements;

begin
   for e := H to F do
      writeln(ElemNames[e]);
end.
