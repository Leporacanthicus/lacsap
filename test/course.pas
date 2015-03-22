program course;

const
   EPS = 1e-15;
   R2D = 57.295779513082320876798154814105;
   D2R =  0.01745329251994329576923690768489;

function CourseInitial(lat1, lon1, lat2, lon2 : real): real;

var
   radLat1, radLat2 : real;
   radDeltaLon	    : real;
   tc		    : real;

begin
   radLat1 := D2R *  lat1;
   radLat2 := D2R *  lat2;
   radDeltaLon := D2R * (lon2 - lon1);

   if cos(radLat1) < EPS then
   begin
      if radLat1 > 0 then 
	 tc := 180
      else 
	 tc := 0;
   end else begin
      tc := R2D * arctan2(sin(radDeltaLon),
			cos(radLat1) * tan(radLat2) - sin(radLat1) * cos(radDeltaLon));
   end;

   if abs(tc) < EPS then
      tc := 0
   else
      tc := tc + 360;

   tc := fmod(tc, 360);

   CourseInitial := tc;
end;

type
   LatLon =  record
		lat, lon : real;
	     end;	 
   
   CoursePoint = record
		    a, b :  LatLon;
		 end;

const
   SIZE	= 100;

var
   cps : array [1..SIZE] of CoursePoint;
   tc  : array [1..SIZE] of real;

function ReadLatLon: LatLon;

var
   l : LatLon;
begin
   read(l.lat, l.lon);
   ReadLatLon := l;
end; { RandomLatLon }

var
   i   : integer;
   t   : longint;
   tot : real;
   cl  : real;
begin
   for i := 1 to SIZE do
   begin
      cps[i].a := ReadLatLon();
      cps[i].b := ReadLatLon();
   end;
   t := cycles;
   for i := 1 to SIZE do
   begin
      tc[i] := CourseInitial(cps[i].a.lat, cps[i].a.lon, cps[i].b.lat, cps[i].b.lon);
   end;

   t := cycles - t;
   cl := t / SIZE;
{   writeln('Time=', cl:8:3); }
   tot := 0;
   for i := 1 to SIZE do
   begin
      tot := tot + tc[i];
      writeln('tc[', i:1, ']=', tc[i]:8:5);
   end;

   writeln('Sum of courses: ', tot:12:2);
end.
