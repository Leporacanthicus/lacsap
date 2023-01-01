program constrec;

type
   planet =  record
		diameter : real;
		volume	 : real;
		area	 : real;
		distance : real;
		number	 : integer;
		name	 : string;
	     end;

const
   mercury = planet[diameter:4879*1000.0; area: pi*4879*4879; volume: pi*4879*4879*4879/(3.0*2); 
		    distance: 57.9*1000; number:1; name:"Mercury"];
   venus = planet[diameter:12104*1000.0; area: pi*12104*12104; volume: pi*12104*12104*12104/(3.0*2);
		  distance: 108.2*1000; number:2; name:"Venus"];
   earth = planet[diameter:12756*1000.0; area: pi*12756*12756; volume: pi*12756*12756*12756/(2*3.0);
		  distance: 147.1*1000; number:3; name:"Earth"];

procedure print_planet(p : planet);

begin
   writeln(p.name, " Diameter[m]:", p.diameter, " Area[km2]: ", p.area, " Volume[km3]: ",
	   p.volume, " Number: ", p.number);
end;

begin
   print_planet(mercury);
   print_planet(venus);
   print_planet(earth);
end.
  
