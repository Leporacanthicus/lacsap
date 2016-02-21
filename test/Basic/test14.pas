program call;
type
   arr5	=  array [0..4] of integer; 
var 
   i1   : integer;
   c1   : char;
   a    : arr5;

procedure test;
begin
   writeln('test')
end;

procedure test2(i : integer);
begin
   writeln(i:1)
end;

procedure test3(i, j : integer);
begin
   writeln(i:1);
   writeln(j:1)
end;

procedure test4(i, j, k : integer);
begin
   writeln(i:1);
   writeln(j:1);
   writeln(k:1)
end;

function test5(i : integer) : integer;
begin
   test5 := i + 23
end;

procedure test6(var i : integer; var c : char);
begin
   writeln(i:1, c);
   i := 123;
   c := 'x';
   writeln(i:1, c)
end;

procedure test7(i : integer; c : char);
begin
   writeln(i:1, c);
   i := 123;
   c := 'x';
   writeln(i:1, c)
end;

procedure test8(var a : arr5 );
begin
   writeln(a[0]:1, a[1]:1, a[2]:1, a[3]:1, a[4]:1);
   a[0] := 11;
   a[1] := 22;
   a[2] := 33;
   a[3] := 44;
   a[4] := 55;
   writeln(a[0]:1, a[1]:1, a[2]:1, a[3]:1, a[4]:1)
end;

procedure test9(a : arr5);
const
    x = 13243;
    y = 31445;
var
    i : integer;
    j : integer;
begin
   i := x;
   j := y;
   writeln(a[0]:1, a[1]:1, a[2]:1, a[3]:1, a[4]:1);
   a[0] := 11;
   a[1] := 22;
   a[2] := 33;
   a[3] := 44;
   a[4] := 55;
   writeln(a[0]:1, a[1]:1, a[2]:1, a[3]:1, a[4]:1);
   writeln(i:1, ' ', j:1)
end;


begin
   test;
   writeln;
   test2(23);
   writeln;
   test3(34, 45);
   writeln;
   test4(56, 67, 78);
   writeln;
   writeln(test5(2):1);

   i1 := 1;
   c1 := 'a';
   writeln(i1:1, c1);
   test6(i1, c1);
   writeln(i1:1, c1);

   i1 := 1;
   c1 := 'a';
   writeln(i1:1, c1);
   test7(i1, c1);
   writeln(i1:1, c1);

   a[0] := 1;
   a[1] := 2;
   a[2] := 3;
   a[3] := 4;
   a[4] := 5;
   writeln(a[0]:0, a[1]:1, a[2]:1, a[3]:1, a[4]:1);
   test8(a);
   writeln(a[0]:1, a[1]:1, a[2]:1, a[3]:1, a[4]:1);

   a[0] := 1;
   a[1] := 2;
   a[2] := 3;
   a[3] := 4;
   a[4] := 5;
   writeln(a[0]:1, a[1]:1, a[2]:1, a[3]:1, a[4]:1);
   test9(a);
   writeln(a[0]:1, a[1]:1, a[2]:1, a[3]:1, a[4]:1)
end.
