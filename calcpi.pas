PROGRAM calcpi(input, output);
(* Original author unknown, courtesy Jesper Wolf for TP4 or TP5 *)
(* Variable length fixed point real calculation                 *)

CONST
   longmax  = 16;
   w_dig    = 4.81647993;  { 16 * log(2) Number of digits per word }
   max_word = 65535;
	    
TYPE
   word	   = 0..max_word;
   longptr = ^long;
   long	   = RECORD
		len : word;
		dat : ARRAY[1..longmax] OF word;
	     END; (* long *)

    pntr           = RECORD
      CASE integer OF
0  : (  p            : ^word);
1  : (  l, h         : word);
      END; (* pntr *)

  VAR
     pi,     
     angle239 : longptr;
     remainder,
     i, l           : word;
     number         : longint;
     nin, link      : longptr;
     words          : word;
     n_max : real;
     t0,t1: longint;

PROCEDURE writelong(VAR v : long); forward;

PROCEDURE addlong(VAR answer, add :  long);

var
   i	 : integer;
   lng	 : integer;
   tmp	 : integer;
   carry : integer;

begin
   lng := add.len;
   carry := 0;
   for i := 1 to lng do
   begin
      tmp := answer.dat[i] + add.dat[i] + carry;
      carry := tmp shr 16;
      answer.dat[i] := tmp and max_word;
   end;
   while carry <> 0 do
   begin
      tmp := answer.dat[i];
      carry := tmp shr 16;
   end;
   WITH answer DO
      IF dat[len+1] <> 0 THEN len := len + 1;
end;


PROCEDURE mullong(VAR a : long; mul : word; VAR answer : long);
var
   i	 : integer;
   lng	 : integer;
   tmp	 : integer;
   high	 : integer;
   high2 : integer;
   carry : integer;

BEGIN (* mullong *)
   lng := a.len;
   high := 0;
   for i := 1 to lng do
   begin
      tmp := a.dat[i] * mul;
      high2  := tmp shr 16;
      tmp := tmp and max_word + high + carry;
      carry := tmp shr 16;
      high := high2;
      answer.dat[i] := tmp and max_word;
   end;
   answer.dat[lng] := carry;
   WITH answer DO
      IF dat[a.len+1] = 0 THEN len := a.len
      ELSE len := a.len + 1;
end;   
   

PROCEDURE divlong(VAR a	     : long; del : word;
		  VAR answer : long; VAR remainder : word);

var
   i   : integer;
   lng : integer;
   tmp : integer;
   pmt : integer;
       
begin
   writelong(a);
   writeln('a.len=', a.len:1);
   writeln('answer.len=', answer.len:1);
   lng := a.len;
   for i := lng downto 1 do
   begin
      tmp := a.dat[i] div del;
      pmt := a.dat[i] mod del;
      answer.dat[i] := tmp;
      writeln('tmp=', tmp:0, ' i=', i:0);
   end;
   remainder := pmt;
   
   WITH answer DO BEGIN
      len := a.len;
      WHILE (dat[len] = 0) AND (len > 1) DO len := len - 1;
   END;
   writeln('answer.len=', answer.len:1);
END; (* divlong *)


PROCEDURE sublong(VAR answer, add : long);

var
   i	 : integer;
   lng	 : integer;
   tmp	 : integer;
   carry : integer;

begin
   lng := add.len;
   carry := 0;
   for i := 1 to lng do
   begin
      tmp := answer.dat[i] - add.dat[i] - carry;
      carry := (tmp shr 16) and 1;
      answer.dat[i] := tmp and max_word;
   end;
   while carry <> 0 do
   begin
      tmp := answer.dat[i] - 1;
      carry := tmp shr 16;
   end;
   WITH answer DO BEGIN
      WHILE (len > 1) AND (dat[len] = 0) DO
	 len := len - 1;
   END;
END; (* sublong *)


PROCEDURE writelong(VAR pi : long);

VAR
   l, l1, r, r1, x , nr	: word;
   i			: longint;
			   
PROCEDURE wr(c : char);

BEGIN (* wr *)
   IF x MOD 6 = 0 THEN
      IF x < 69 THEN BEGIN
	 write(' ');
	 x := x+1;
      END ELSE BEGIN
	 IF nr MOD 5 = 0 THEN writeln;
	 nr := nr + 1;
	 writeln;
	 write('            ');
	 x := 13;
      END;
   write(c); x := x+1;
END; (* wr *)


BEGIN 
   nr := 1;
   WITH pi DO BEGIN
      l := len; l1 := l - 1; i := 1;
      write('   Pi  =  ', dat[l], '.'); x := 13;
      FOR i := 1 TO number DIV 4 DO BEGIN
	 (*   IF dat[i] = 0 THEN i := i + 1; *)
	 dat[l] := 0; len := l1;
	 mullong(pi, 10000, pi);
	   r := dat[l];
	 r1 := r DIV 100; r := r MOD 100;
	 wr(chr(r1 DIV 10 + 48)); wr(chr(r1 MOD 10 + 48));
	 wr(chr(r DIV 10 + 48));  wr(chr(r MOD 10 + 48)); END;
   END;
   writeln;
END;

PROCEDURE zerolong(VAR l : long; size : word);

var i : integer;
BEGIN (* zerolong *)
   for i := 1 to size do
      l.dat[i] := 0;
   l.len := words;
END; (* zerolong *)
   
PROCEDURE copylong(VAR src, dest : long);

var i : integer;
BEGIN (* copylong *)
   for i := 1 to words do
   begin
      dest.dat[i] := src.dat[i];
      writeln('Copy:', dest.dat[i]:2);
   end;
   dest.len := src.len;
END; (* copylong *)

PROCEDURE getlong(VAR p : longptr; size : word);

BEGIN (* getlong *)
   new(p);
END; (* getlong *)


PROCEDURE arccotan(n : word; VAR angle : long);

VAR
   n2, del,	
   remainder	: word;
   positive	: boolean;
   
BEGIN (* arccotoan *)
   zerolong(angle, words);
   zerolong(nin^, words);
   zerolong(link^, words);

   writeln('angle.len=', angle.len);
   angle.dat[angle.len] := 1;
   writeln('angle.len=', angle.len);
   divlong(angle, n, angle, remainder);
   write('Angle:');
   writelong(angle);
   writeln;
   
   n2 := n*n;
   del := 1;
   positive := true;
   
   copylong(angle, nin^);
   
   REPEAT
      write('Before:');
      writelong(nin^);
      writeln;
      divlong(nin^, n2, nin^, remainder);
      writeln('After divlong: rem', remainder, ' pos:', positive);
      writelong(nin^);
      del := del + 2;
      positive := NOT positive;
      divlong(nin^, del, link^, remainder);
      writeln('After divlong2: rem', remainder, ' pos:', positive);
      writelong(nin^);
      IF positive THEN
	 addlong(angle, link^)
      ELSE
	 sublong(angle, link^);
      writeln('After add/sub, angle:');
      writelong(angle);
   UNTIL (link^.len <= 1) AND (link^.dat[1] = 0);
END; (* arccotan *)

(* 1---------------1 *)

BEGIN (* calcpi *)
   n_max := longmax * w_dig;
   write('Digits (max ', n_max: 0: 0, ') : ');
   readln(number);
   t0 := clock;
   number := round(number / 4) * 4;
   words := round(number / w_dig + 2);
   
   getlong(pi, words+2);
   getlong(angle239, words+2);
   getlong(link, words+2);
   getlong(nin, words+2);
   
   arccotan(5, pi^);                      { ATan(1/5) }
   addlong(pi^, pi^);  { * 2       }
   addlong(pi^, pi^);  { * 4       }

   arccotan(239, angle239^);
   sublong(pi^, angle239^);
   addlong(pi^, pi^);  { * 2       }
   addlong(pi^, pi^);  { * 4       }
   t1 := clock;
   writeln;
   writelong(pi^);
   writeln;
   
   writeln(t1 - t0:1, ' MicroSeconds to calculate ',
	   number, ' Decimals of pi');
END. (* calcpi *)
