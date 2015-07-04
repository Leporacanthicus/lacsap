{ ******************************************************************
    Mersenne Twister Random Number Generator for pascal
  ******************************************************************}
{* From http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/VERSIONS/PASCAL/Alex.pas *}
program mtprog;

type
   Int32 = integer;
   Int16 = 0..65535;
   Cardinal = Int32;
const
   N	      = 624;
   N_1	      = 623;
   M	      = 397;
   MATRIX_A   = $9908b0df;  { constant vector a }
   UPPER_MASK = $80000000;  { most significant w-r bits }
   LOWER_MASK = $7fffffff;  { least significant r bits }
	      

var
   mt	 : array[0..N_1] of Cardinal{Int32};  { the array for the state vector }
   mti	 : Int16;                        { mti == N+1 means mt[N] is not initialized }
   mag01 : array[0..1] of Cardinal{Int32};
   init	 : array[0..N] of Int32;
   i	 : integer;
   
procedure InitMT(Seed : Int32);
var
  i : Int16;
begin
  mt[0] := Seed and $ffffffff;
  for i := 1 to N_1 do
    begin
      mt[i] := (1812433253 * (mt[i-1] Xor (mt[i-1] shr 30)) + i);
        { See Knuth TAOCP Vol2. 3rd Ed. P.106 For multiplier.
          In the previous versions, MSBs of the seed affect
          only MSBs of the array mt[].
          2002/01/09 modified by Makoto Matsumoto }
      mt[i] := mt[i] and $ffffffff;
        { For >32 Bit machines }
    end;
  mti := N;
end; { InitMT }

type
   arrNLong = array [0..N] of Int32;

procedure InitMTbyArray(InitKey : arrNLong ; KeyLength : Int16);
var
  i, j, k, k1 : Int16;
begin
  InitMT(19650218);

  i := 1;
  j := 0;

  if N > KeyLength then k1 := N else k1 := KeyLength;

  for k := k1 downto 1 do
  begin
      mt[i] := (mt[i] Xor ((mt[i-1] Xor (mt[i-1] shr 30)) * 1664525)) + InitKey[j] + j; { non linear }
(*      mt[i] := mt[i] and $ffffffff; { for WORDSIZE > 32 machines } *)
      i := i + 1;
      j := j + 1;
      if i >= N then
        begin
          mt[0] := mt[N-1];
          i := 1;
        end;
      if j >= KeyLength then j := 0;
    end;

  for k := N-1 downto 1 do
    begin
      mt[i] := (mt[i] Xor ((mt[i-1] Xor (mt[i-1] shr 30)) * 1566083941)) - i; { non linear }
     (* mt[i] := mt[i] and $ffffffff; { for WORDSIZE > 32 machines } *)
      i := i + 1;
      if i >= N then
        begin
          mt[0] := mt[N-1];
          i := 1;
        end;
    end;

    mt[0] := $80000000; { MSB is 1; assuring non-zero initial array }
end;

function IRanMT : Int32;
var
  y : Int32;
  k : Int16;
begin
  if mti >= N then  { generate N words at one Time }
    begin
      { If IRanMT() has not been called, a default initial seed is used }
      if mti = N + 1 then InitMT(5489);

      for k := 0 to (N-M)-1 do
        begin
          y := (mt[k] and UPPER_MASK) or (mt[k+1] and LOWER_MASK);
          mt[k] := mt[k+M] xor (y shr 1) xor mag01[y and $1];
        end;

      for k := (N-M) to (N-2) do
        begin
          y := (mt[k] and UPPER_MASK) or (mt[k+1] and LOWER_MASK);
          mt[k] := mt[k - (N - M)] xor (y shr 1) xor mag01[y and $1];
        end;

      y := (mt[N-1] and UPPER_MASK) or (mt[0] and LOWER_MASK);
      mt[N-1] := mt[M-1] xor (y shr 1) xor mag01[y and $1];

      mti := 0;
    end;

  y := mt[mti];
  mti := mti + 1;

  { Tempering }
  y := y xor (y shr 11);
  y := y xor ((y shl  7) and $9d2c5680);
  y := y xor ((y shl 15) and $efc60000);
  y := y xor (y shr 18);

  IRanMT := y
end;


begin
   mag01[0] := 0;
   mag01[1] := MATRIX_A;
   init[0] := $123;
   init[1] := $234;
   init[2] := $345;
   init[3] := $456;
   InitMTbyArray(init, 4);
   writeln('1000 outputs of genrand_int32()');
   for i := 1 to 1000 do
   begin
      write(IRanMT:11, ' ');
      if i mod 5 = 0 then
	 writeln;
   end;
end.

{ ******************************************************************
    Mersenne Twister Random Number Generator end
  ******************************************************************}
