program SieveOfEratosthenes;

const
   maxNum = 100000;
   nLoops = 10000;

type
   int32 =  integer;

procedure Sieve;
var
   Data: array [1..maxNum] of boolean;
    i,j: int32;
begin
   for i := 1 to maxNum do
      Data[i] := true;

   for i := 2 to maxNum do
   begin
      if Data[i] then
      begin
	 j := i+i;
	 while j<=maxNum do
	 begin
	    Data[j] := false;
	    j := j + i;
	 end;
      end;
   end;
end;

var
   i : integer;

begin
   for i := 1 to nLoops do
   begin
      Sieve;
   end; { SieveOfEratosthenes }
end.
