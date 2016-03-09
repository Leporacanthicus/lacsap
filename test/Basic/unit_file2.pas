unit unit_file2;

interface
type
   o = object
         procedure pp;
        end;

procedure pp;

implementation

procedure o.pp;
begin
   writeln('bad');
end; { o }

procedure pp;

begin
   writeln('good');
end;

end.
