program binops;

(* Test invalid binary operations *)

begin
   writeln('a' - 'b');
   writeln('a' and 'b');
   writeln('a' xor 'b');
   writeln('a' or 'b');
   writeln('a' * 'b');
   writeln('a' shl 'b');
   writeln('a' shr 'b');
end.
