program linkedlist;

type
   Link	     = ^ListEntry;
   ListEntry = record
		  next : Link;
		  x    : integer;
	       end;


var
   head	: Link;
   i	: integer;

procedure addLink(x : integer);

var
   t : Link;
   
begin
   new(t);
   t^.next := nil;
   t^.x := x;
   if head = nil then
   begin
      head := t;
   end
   else
   begin
      t^.next := head;
      head := t;
   end;
end; { addLink }


procedure print;

var
   t : Link;

begin
   t := head;
   while t <> nil do
   begin
      writeln(t^.x : 1);
      t := t^.next;
   end;
end;   

begin
   for i := 1 to 10 do
      addLink(i);
   print;
end.
