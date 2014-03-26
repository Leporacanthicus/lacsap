program copyfile2;

type
   filetype = file of integer;

var
   infile, outfile : filetype;
   size		   : integer;

begin
   size := 0;
   assign(infile, 'infile.dat');
   reset(infile);
   assign(outfile, 'outfile.dat');
   rewrite(outfile);
   while not eof(infile) do
   begin
      outfile^ := infile^;
      get(infile);
      put(outfile);
      size := size + 1;
   end;
   close(infile);
   close(outfile);
   writeln(size:1, ' elements have been copied');
end.
      
   
