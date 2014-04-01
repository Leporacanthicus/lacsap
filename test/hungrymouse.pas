program hungrymouse;

const 
   nmax	       = 4;
   nrofdoors   = 4;
   nrofevents  = 3;
   nrofchoices = 40;

type
   slhvector = array [1..nmax] of real;
   smatrix   = array [1..nrofdoors] of slhvector;

var
   door, event, choice : integer;
   doorprob	       : slhvector;
   eventprob	       : smatrix;
   factor	       : array [1..nrofevents] of real;

function val(var p : slhvector; n:integer):integer;
var
   slump, psum : real;
   k	       : integer;

begin
   slump := random;
   k := 0;
   psum := 0;
   repeat
      k := k + 1;
      psum := psum + p[k];
   until (psum > slump) or (k = n);
   val := k;
end; { val }

procedure update(var p : slhvector; n, index:integer; factor:real);
var
   i	   : integer;
   divisor : real;

begin
   divisor := 1 + (factor-1)*p[index];
   p[index] := factor*p[index];
   for i := 1 to n do
      p[i] := p[i]/divisor;
end; { update }

begin
   for door := 1 to nrofdoors do
      doorprob[door] := 1/nrofdoors;
   for door := 1 to nrofdoors do
      for event := 1 to nrofevents do
	 read(eventprob[door,event]);
   for event := 1 to nrofevents do
      read(factor[event]);

   writeln('Probability for the mouse to select door no:');
   for door := 1 to nrofdoors do
      write(door:10);
   writeln;

   for door := 1 to nrofdoors do
      write(doorprob[door]:10:3);
   writeln;

   for choice := 1 to nrofchoices do
   begin
      door := val(doorprob, nrofdoors);
      event := val(eventprob[door], nrofevents);

      update(doorprob, nrofdoors, door, factor[event]);
      for door := 1 to nrofdoors do
	 write(doorprob[door]:10:3);
      writeln;
   end;
end.
   

