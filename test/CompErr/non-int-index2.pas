program nonintinit;

const
   limit         = 43;
   unity         = 1.0; 

type
   indextype   = 1..limit; 
   vector      = array [indextype] of real; 
   UnitVector    = vector[1..2.0: unity otherwise 0]; 

begin
end.
