program copyfile;

type
   filetype = file of integer;

var
   infile, outfile : filetype;
   element	   : integer;
   size		   : integer;

begin
   size := 0;
   assign(infile, 'infile.dat');
   reset(infile);
   assign(outfile, 'outfile.dat');
   rewrite(outfile);
   while not eof(infile) do
   begin
      read(infile, element);
      write(outfile, element);
      writeln('element=', element, ' count=', size);
      size := size + 1;
   end;
   close(infile);
   close(outfile);
   writeln(size:1, ' elements have been copied');
end.
      
   
