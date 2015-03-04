program sudoku;

type
   GridEntry  = record
		   fixed : boolean;
		   n	 : integer;
		end;
   
   GridArray  = array [1..9, 1..9] of GridEntry;
   
   CandType   = (Row, Col, Sub, Overall);
   
   CandSet    = set of 1..9;
   
   CandList   = record
		   cursor : integer;
		   bits	  : CandSet;
		end;
   
   Candidates = record
		   row : array [1..9] of CandSet;
		   col : array [1..9] of CandSet;
		   sub : array [1..3, 1..3] of CandSet;
		end;
   
   Game	      = record
		   grid	      : GridArray;
		   time	      : real;
		   solved     : boolean;
		   cand	      : Candidates;
		   emptyCount : integer;
		end;	      

var
   g : Game;
   i : integer;

procedure SubDiv(x, y : integer; var sx, sy : integer);

begin
   sx := (x-1) div 3 + 1;
   sy := (y-1) div 3 + 1;
end; { SubDiv }

function Invert(s : CandSet): CandSet;

var
   c : CandSet;
   i : integer;

begin
   c := [];
   for i := 1 to 9 do
      if not (i in s) then
	 c := c + [i];
   Invert := c;
   {
   Invert := [1..9] - s;
   }
end;

procedure ClearGame(var g : Game);

var
   x, y	  : integer;
   sx, sy : integer;
begin
   g.emptyCount := 81;
   for y := 1 to 9 do
   begin
      for x := 1 to 9 do
      begin
	 g.grid[x,y].n := 0;
	 g.grid[x,y].fixed := false;
	 SubDiv(x, y, sx, sy);
	 g.cand.sub[sx, sy] := [1..9];
	 g.cand.col[x] := [1..9];
      end;
      g.cand.row[y] := [1..9];
   end;
end;

function GetCandList(var g : Game;  x, y : integer; ty : CandType) : CandList;

var
   sx, sy : integer;
   tmp	  : CandList;
   
begin
   tmp.cursor := 0;
   case ty of
     Sub :
	  begin
	     SubDiv(x, y, sx, sy);
	     tmp.bits := g.cand.sub[sx, sy];
	  end;
     
     Col : 
	   tmp.bits := g.cand.col[x];
     Row : 
	   tmp.bits := g.cand.row[y];
     Overall :
	      begin
		 SubDiv(x, y, sx, sy);
		 tmp.bits := g.cand.row[y] * g.cand.col[x] * g.cand.sub[sx, sy];
	      end;
   end;
   GetCandList := tmp;
end; { GetCandList }

function CountCandidates(cand : CandList) : integer;

var
   i, n : integer;

begin
   CountCandidates := popcnt(cand.bits);
{   n := 0;
   for i := 1 to 9 do
      if i in cand.bits then
	 n := n + 1;
   CountCandidates := n; }
end; { CountCandidates }


procedure SetGrid(var g: Game; x, y: integer; n: integer; fixed : boolean);

var
   sx, sy : integer;

begin
   SubDiv(x, y, sx, sy);
   if g.grid[x, y].n <> 0 then
   begin
      writeln("Setting cell which already has a value @ ", x:1, "," , y:1);
      panic("Exiting");
   end;

   g.grid[x, y].n := n;
   g.grid[x, y].fixed := fixed;
   g.emptyCount := g.emptyCount - 1;
   with g.cand do
   begin
      row[y] := row[y] - [n];
      col[x] := col[x] - [n];
      sub[sx, sy] := sub[sx, sy] - [n];
   end;
end; { SetGrid }


procedure SetGridFromString(var g : Game; row: integer; str: string);

var
   i : integer;

begin
   if length(str) <> 9 then
      panic("String not right length");
   for i := 1 to 9 do
      if str[i] <> '0' then
	 SetGrid(g, i, row, ord(str[i]) - ord('0'), true);
end;

procedure InitGame(var g : Game; index: integer);

const
   maxindex =  7;

begin
   ClearGame(g);

   case (index mod maxindex)+1 of
     1 : begin
	    SetGridFromString(g, 1, '700004060');
	    SetGridFromString(g, 2, '120009000');
   	    SetGridFromString(g, 3, '000507203');
	    
	    SetGridFromString(g, 4, '003000700');
	    SetGridFromString(g, 5, '800000009');
	    SetGridFromString(g, 6, '004000600');
	    
	    SetGridFromString(g, 7, '901803000');
	    SetGridFromString(g, 8, '000900051');
	    SetGridFromString(g, 9, '040700002');
	 end;
     2 : begin
	    SetGridFromString(g, 1, '000010005');
	    SetGridFromString(g, 2, '035000827');
	    SetGridFromString(g, 3, '009008000');
	    
	    SetGridFromString(g, 4, '406030009');
	    SetGridFromString(g, 5, '000902160');
	    SetGridFromString(g, 6, '070400000');
	    
	    SetGridFromString(g, 7, '013020900');
	    SetGridFromString(g, 8, '060007008');
	    SetGridFromString(g, 9, '002800540');
	 end;
     3 : begin
	    SetGridFromString(g, 1, '200908004');
	    SetGridFromString(g, 2, '070000020');
	    SetGridFromString(g, 3, '005000800');
	    
	    SetGridFromString(g, 4, '500206007');
	    SetGridFromString(g, 5, '020010030');
	    SetGridFromString(g, 6, '900704005');
	    
	    SetGridFromString(g, 7, '004000700');
	    SetGridFromString(g, 8, '090000060');
	    SetGridFromString(g, 9, '600407002');
	 end;
     4 : begin
	    SetGridFromString(g, 1, '000010003');
	    SetGridFromString(g, 2, '030800040');
	    SetGridFromString(g, 3, '009320006');
	    
	    SetGridFromString(g, 4, '500001900');
	    SetGridFromString(g, 5, '007000500');
	    SetGridFromString(g, 6, '008700001');
	    
	    SetGridFromString(g, 7, '200073800');
	    SetGridFromString(g, 8, '040008010');
	    SetGridFromString(g, 9, '700060000');
	 end;
     5 : begin
	    SetGridFromString(g, 1, '600500010');
	    SetGridFromString(g, 2, '300090000');
	    SetGridFromString(g, 3, '071060003');
	    
	    SetGridFromString(g, 4, '089000000');
	    SetGridFromString(g, 5, '200104006');
	    SetGridFromString(g, 6, '000000830');
	    
	    SetGridFromString(g, 7, '700010620');
	    SetGridFromString(g, 8, '000050004');
	    SetGridFromString(g, 9, '020006007');
	 end;
     6 : begin
	    SetGridFromString(g, 1, '065900008');
	    SetGridFromString(g, 2, '000000691');
	    SetGridFromString(g, 3, '000002000');

	    SetGridFromString(g, 4, '208700506');
	    SetGridFromString(g, 5, '000000000');
	    SetGridFromString(g, 6, '406003207');

	    SetGridFromString(g, 7, '000400000');
	    SetGridFromString(g, 8, '547000000');
	    SetGridFromString(g, 9, '100008750');
	 end;
     7 : begin
	    SetGridFromString(g, 1, '000501000');
	    SetGridFromString(g, 2, '030000080');
	    SetGridFromString(g, 3, '004200500');

	    SetGridFromString(g, 4, '500000602');
	    SetGridFromString(g, 5, '060050030');
	    SetGridFromString(g, 6, '809004005');

	    SetGridFromString(g, 7, '002003100');
	    SetGridFromString(g, 8, '080090070');
	    SetGridFromString(g, 9, '000406000');
	 end;
     otherwise : 
      	 panic("Unknown index");
   end;	       
end; { InitGame }


function Empty(var g : Game; x, y: integer): boolean;

begin
   Empty := g.grid[x, y].n = 0;
end;


function FindCandidate(var g : Game;  var ax, ay: integer; var list: CandList): boolean;

var
   x, y	  : integer;
   sx, sy : integer;
   cx, cy : integer;
   lowest : integer;
   count  : integer;
   cand	  : CandList;
   cand2  : CandList;
   cand3  : CandList;
   inv	  : CandList;
   done	  : boolean;
   
begin
   list.cursor := 0;
   list.bits   := [];
   FindCandidate := true;
   lowest := 10;
   y := 1;
   done := false;
   repeat
      x := 1;
      repeat
	 if Empty(g, x, y) then
	 begin
	    cand := GetCandList(g, x, y, Overall);
	    count := CountCandidates(cand);
	    if count = 0 then
	    begin
	       FindCandidate := false;
	       done := true
	    end
	    else if count = 1 then
	    begin
	       ax := x;
	       ay := y;
	       list.bits := cand.bits;
	       done := true;
	    end
	    else
	    begin
	       cand2 := cand;
	       SubDiv(x, y, sx, sy);
	       cx := (x-1) div 3 * 3 + 1;
	       cy := (y-1) div 3 * 3 + 1;
	       for sy := 0 to 2 do
		  for sx := 0 to 2 do
		     if Empty(g, sx + cx, sy + cy) then
			if (sx + cx <> x) or (sy + cy <> y) then
			begin
			   cand3 := GetCandList(g, sx + cx, sy + cy, Overall);
			   cand2.bits := cand2.bits * Invert(cand3.bits);
			end;
	       if CountCandidates(cand2) = 1 then
	       begin
		  done := true;
		  list.bits := cand2.bits;
		  ax := x;
		  ay := y;
	       end
	       else if count < lowest then
	       begin
		  ax := x;
		  ay := y;
		  list.bits := cand.bits;
		  lowest := count;
	       end;
	    end;
	 end;
	 x := x + 1;
      until done or (x = 10);
      y := y + 1;
   until done or (y = 10);
end; { FindCandidate }


function Next(var list : CandList): integer;

var
   res	: integer;
   
begin
   res := 0;
   with list do
      while (cursor <= 9) and (res = 0) do
      begin
	 cursor := cursor + 1;
	 if cursor in bits then
	    res := cursor;
      end;
   Next := res;
end;


procedure DrawGame(var g : Game);

var
   x, y	: integer;
   
begin
   for y := 1 to 9 do
   begin
      for x := 1 to 9 do
      begin
	 with g.grid[x, y] do
	 begin
	    write('|');
	    if n = 0 then
	       write('   ')
	    else
	       write(n : 2, ' ');
	 end;
      end;
      writeln('|');
   end;
end; { DrawGame }


function Solve(var g : Game; level: integer): boolean;

var
   x, y	: integer;
   list	: CandList;
   f	: boolean;
   done	: boolean;
   val	: integer;
   gm	: Game;

begin
   done := false;
   Solve := false;
   repeat
      if FindCandidate(g, x, y, list) then 
      begin
	 if g.emptyCount = 0 then
	 begin
	    g.solved := true;
	    done := true;
	    Solve := true;
	 end;
      end
      else
	 done := true;
      if not done then
      begin
	 val := Next(list);
	 if CountCandidates(list) = 1 then
	 begin
	    SetGrid(g, x, y, val, false);
	    if g.emptyCount = 0 then
	    begin
	       g.solved := true;
	       Solve := true;
	       done := true;
	    end
	 end
	 else
	 begin
	    while not done and (val <> 0) do
	    begin
	       gm := g;
	       SetGrid(gm, x, y, val, false);
	       if g.emptyCount = 0 then
	       begin
		  done := true;
		  g.solved := true;
		  Solve := true;
	       end
	       else if Solve(gm, level+1) then
	       begin
		  g := gm;
		  done := true;
		  Solve := true;
	       end;
	       val := Next(list);
	    end;
	    done := true;
	 end;
      end;
   until done;
end;

begin
   for i := 1 to 7 do
   begin
      InitGame(g, i);
      DrawGame(g);
      if Solve(g, 1) then
	 writeln("Success")
      else
	 writeln("failed");
      DrawGame(g);
   end;
   for i := 1 to 40 do
   begin
      InitGame(g, i);
      if not Solve(g, 1) then
	 writeln("failed");
   end;      
end.
