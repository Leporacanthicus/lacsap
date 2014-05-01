program variant;

type
   date	  = record
	       year  : integer;
	       month : 1..12;
	       day   : 1..31;
	    end;
   str	  = array [1..15] of char;
   person = record   
	       name  : str;
	       dob   : date;
	       case native: boolean of
		 true  : (birthplace: str);
		 false : (countryofbirth: string;
			  naturalized: date);
	    end;

var
   p1, p2 : person;


procedure PrintPerson(var p : person);
begin
   write(p.name);
   case p.native of
     true  : writeln(' is native.');
     false : writeln(' naturalized on ', p.naturalized.day:3, p.naturalized.month:3, p.naturalized.year:5);
   end; { case }
end;

begin
   p1.name :=       'Jacob Adamson  ';
   p1.dob.year :=   1964;
   p1.dob.month :=  2;
   p1.dob.day :=    29;
   p1.native :=     true;
   p1.birthplace := 'Stockholm';

   p2.name :=           'Adam Jacobson  ';
   p2.dob.year :=       1964;
   p2.dob.month :=      9;
   p2.dob.day :=        22;
   p2.native :=         false;
   p2.countryofbirth := 'USA';
   p2.naturalized.year := 1982;     
   p2.naturalized.month := 12;
   p2.naturalized.day := 13;

   PrintPerson(p1);
   PrintPerson(p2);
end.
