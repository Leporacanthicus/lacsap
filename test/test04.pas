program Fibonacci;

var
	fibo1, fibo2 : integer ;
	temp, cnt : integer ;

begin
	writeln('Primele zece numere Fibonacci:');
	cnt := 0 ;
	fibo1 := 0 ;
	fibo2 := 1 ;
	while cnt < 10 do
	begin
	   write(fibo2:1, ' ');
		temp := fibo2 ;
		fibo2 := fibo2 + fibo1 ; 
		fibo1 := temp ;
		cnt := cnt + 1
	end ;
	writeln 
end.
