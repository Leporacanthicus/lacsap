program records(output);

{ Borrowed from Gnu Pascal tests }
type
        cmplx = record
                rp: real;
                ip: real;
        end;
var
        x, y: cmplx;
function cadd(a,b: cmplx): cmplx;
        var res: cmplx;
        begin
                res.rp := a.rp + b.rp;
                res.ip := a.ip + b.ip;
                cadd := res;
        end;
procedure cprint(a: cmplx);
        begin
	   writeln(a.rp:12:8, '+', a.ip:12:8, 'i');
        end;
begin
        x.rp := 1;
        x.ip := 2;
        y.rp := 3;
        y.ip := 4;
        cprint(cadd(x, y));
end.
