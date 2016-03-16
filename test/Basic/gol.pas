program gameoflife;

{ Conways game of life

    Any live cell with fewer than two live neighbours dies, as if caused by under-population.
    Any live cell with two or three live neighbours lives on to the next generation.
    Any live cell with more than three live neighbours dies, as if by over-population.
    Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction.

 }

const
   size	= 50;

type
   sizetype = 1..size;
   gridtype = array [1..size, 1..size] of boolean;

var
   grid	      : gridtype;
   generation : integer;
   maxgen     : integer;

procedure drawgrid(var g :  gridtype);

var
   x,y : integer;

begin
   for y := 1 to size do
   begin
      for x := 1 to size do
	 if g[x][y] then
	    write('x')
	 else
	    write(' ');
      writeln;
   end;
end; { drawgrid }


function neighbours(var g : gridtype; x, y:integer): integer;

var
   count: integer;
   
begin
   count := 0;
   if (y < size) and g[x][y+1] then
      inc(count);
   if (y > 1) and g[x][y-1] then
      inc(count);
   if x > 1 then
   begin
      if g[x-1][y] then
	 inc(count);
      if (y > 1) and g[x-1][y-1] then
	 inc(count);
      if (y < size) and g[x-1][y+1] then
	 inc(count);
   end;
   if x < size then begin
      if g[x+1][y] then
	 inc(count);
      if (y > 1) and g[x+1][y-1] then
	 inc(count);
      if (y < size) and g[x+1][y+1] then
	 inc(count);
   end;
   neighbours := count;
end;

procedure update(var g : gridtype);

var
   x, y	   : integer;
   nextgen : gridtype;
   nb	   : integer;
   b	   : boolean;

begin
   for y := 1 to size do
      for x := 1 to size do
      begin
	 nb := neighbours(g, x, y);
	 b := false;
	 if g[x][y] and (nb in [2,3]) then
	    b := true;
	 if not g[x][y] and (nb = 3) then
	    b := true;
	 nextgen[x][y] := b;
      end;
   g := nextgen;
end;

begin
   generation := 0;
   grid[19][20] := true;
   grid[20][20] := true;
   grid[21][20] := true;
   grid[20][21] := true;
   grid[20][22] := true;
   grid[19][23] := true;
   grid[20][23] := true;
   grid[21][23] := true;

   grid[19][25] := true;
   grid[20][25] := true;
   grid[21][25] := true;
   grid[19][26] := true;
   grid[20][26] := true;
   grid[21][26] := true;

   grid[19][28] := true;
   grid[20][28] := true;
   grid[21][28] := true;
   grid[20][29] := true;
   grid[20][30] := true;
   grid[19][31] := true;
   grid[20][31] := true;
   grid[21][31] := true;
   
   readln(maxgen);
   repeat
      generation := generation + 1;
      drawgrid(grid);
      update(grid);
   until generation > maxgen;
end.
