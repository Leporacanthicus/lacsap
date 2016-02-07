program SieveOfEratosthenes;

const
   maxNum = 1000000000;
   nLoops = 1;

var
   Data : array [1..maxNum] of boolean;

procedure Sieve;
var
    i,j: LongInt;
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
