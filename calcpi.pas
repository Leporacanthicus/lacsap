PROGRAM calcpi(input, output);
(* Original author unknown, courtesy Jesper Wolf for TP4 or TP5 *)
(* Variable length fixed point real calculation                 *)

USES dos;

  CONST
    longmax        = 32766;
    w_dig          = 4.81647993;  { 16 * log(2) Number of digits per word }
    n_max          = longmax * w_dig;

  TYPE
    longptr        = ^long;
    long           = RECORD
      len            : word;
      dat            : ARRAY[1..longmax] OF word;
      END; (* long *)

    pntr           = RECORD
      CASE byte OF
0  : (  p            : ^word);
1  : (  l, h         : word);
      END; (* pntr *)

    tid            = RECORD
      time, minut,
      sek, hsek      : integer
      END; (* tid *)

  (**********************************************************)
  (* To make it possible to use maximally large arrays, and *)
  (* to increase the speed of the computations, all records *)
  (* of type Long MUST start at a segment boundary!         *)
  (**********************************************************)

  VAR
    pi,
    angle239       : longptr;
    remainder,
    i, l           : word;
    number         : longint;
    tid1, tid2     : tid;
    nin, link      : longptr;
    words          : word;

  (* 1---------------1 *)

  PROCEDURE gettime(VAR tiden : tid);
  { substitute for dos.gettime }

    VAR
      regs           : registers;

    BEGIN (* gettime *)
    regs.ax := $2c00; intr($21, regs);
    WITH tiden DO BEGIN
      time := hi(regs.cx); minut := lo(regs.cx);
      sek := hi(regs.dx); hsek := lo(regs.dx); END
    END; (* gettime *)

  (* 1---------------1 *)

  FUNCTION calctime ( tid0 : tid; VAR tid1 : tid) : real;
  (* calctime subtracts two time specifications from each other, *)
  (* the result is returned in both seconds (function result)    *)
  (* and formated as HMS in  the second parameter.               *)

    BEGIN (* calctime *)
    WITH tid1 DO BEGIN
      hsek := hsek - tid0.hsek;
      IF hsek < 0 THEN BEGIN
        sek := sek-1; hsek := hsek+100; END;
      sek := sek - tid0.sek;
      IF sek < 0 THEN BEGIN
        minut := minut-1; sek := sek+60; END;
      minut := minut - tid0.minut;
      IF minut < 0 THEN BEGIN
        time := time-1; minut := minut+60; END;
      time := time - tid0.time;
      IF time < 0 THEN time := time+24;
      calctime := (time*60.0+minut)*60.0+sek+hsek/100; END;
    END; (* calctime *)

  (* 1---------------1 *)

  PROCEDURE addlong(VAR answer, add : long);

    BEGIN (* addlong *)
    inline($fc/                  { CLD                      }
           $1e/                  { PUSH    DS               }
           $8e/$5e/<answer+2/    { MOV     DS,[BP+08]       }
           $c4/$76/<add/         { LES     SI,[BP+06]       }
           $26/$8b/$0e/$00/$00/  { MOV     CX,ES:[0000]     }
           $83/$c6/$02/          { ADD     SI,2             }
           $f8/                  { CLC                      }
           $26/$ad/              { LODSW   ES:              }
           $11/$44/$fe/          { ADC     [SI-02],AX       }
           $e2/$f9/              { LOOP    010E             }
           $73/$07/              { JNB     011E             }
           $83/$04/$01/          { ADD Word Ptr [SI],1      }
           $46/                  { INC     SI               }
           $46/                  { INC     SI               }
           $eb/$f7/              { JMP     0115             }
          $1f);                  { POP     DS               }
    WITH answer DO
      IF dat[len+1] <> 0 THEN len := len + 1;
    END; (* addlong *)

  (* 1---------------1 *)

  PROCEDURE mullong(VAR a : long; mul : word; VAR answer : long);

    BEGIN (* mullong *)
    inline($fc/                  { CLD                      }
           $1e/                  { PUSH    DS               }
           $8e/$5e/<a+2/         { MOV     DS,[BP+0C]       }
           $8b/$5e/<mul/         { MOV     BX,[BP+08]       }
           $c4/$7e/<answer/      { LES     DI,[BP+04]       }
           $8b/$0e/$00/$00/      { MOV     CX,[0000]        }
           $33/$d2/              { XOR     DX,DX            }
           $8b/$f2/              { MOV     SI,DX            }
           $47/                  { INC     DI               }
           $47/                  { INC     DI               }
           $f8/                  { CLC                      }
           $8b/$05/              { MOV     AX,[DI]          }
           $9c/                  { PUSHF                    }
           $f7/$e3/              { MUL     BX               }
           $9d/                  { POPF                     }
           $13/$c6/              { ADC     AX,SI            }
           $ab/                  { STOSW                    }
           $8b/$f2/              { MOV     SI,DX            }
           $e2/$f3/              { LOOP    0114             }
           $83/$d6/$00/          { ADC     SI,+00           }
           $26/$89/$35/          { MOV     ES:[DI],SI       }
           $1f);                 { POP     DS               }
    WITH answer DO
      IF dat[a.len+1] = 0 THEN len := a.len
      ELSE len := a.len + 1;
    END; (* mullong *)

  (* 1---------------1 *)

  PROCEDURE divlong(VAR a : long; del : word;
                    VAR answer : long; VAR remainder : word);

    BEGIN (* divlong *)
    inline($fd/                  { STD                      }
           $1e/                  { PUSH    DS               }
           $8e/$5e/<a+2/         { MOV     DS,[BP+10]       }
           $8b/$5e/<del/         { MOV     BX,[BP+0C]       }
           $c4/$7e/<answer/      { LES     DI,[BP+08]       }
           $8b/$0e/$00/$00/      { MOV     CX,[0000]        }
           $03/$f9/              { ADD     DI,CX            }
           $03/$f9/              { ADD     DI,CX            }
           $33/$d2/              { XOR     DX,DX            }
           $8b/$05/              { MOV     AX,[DI]          }
           $f7/$f3/              { DIV     BX               }
           $ab/                  { STOSW                    }
           $e2/$f9/              { LOOP    0117             }
           $c5/$76/<remainder/   { LDS     SI,[BP+04]       }
           $89/$14/              { MOV     [SI],DX          }
           $1f);                 { POP     DS               }
    WITH answer DO BEGIN
      len := a.len;
      WHILE (dat[len] = 0) AND (len > 1) DO len := len - 1; END;
    END; (* divlong *)

  (* 1---------------1 *)

  PROCEDURE sublong(VAR answer, add : long);

    BEGIN (* sublong *)
    inline($fc/                  { CLD                      }
           $1e/                  { PUSH    DS               }
           $8e/$5e/<answer+2/    { MOV     DS,[BP+08]       }
           $c4/$76/<add/         { LES     SI,[BP+06]       }
           $26/$8b/$0e/$00/$00/  { MOV     CX,ES:[0000]     }
           $83/$c6/$02/          { ADD     SI,2             }
           $f8/                  { CLC                      }
           $26/$ad/              { LODSW   ES:              }
           $19/$44/$fe/          { SBB     [SI-02],AX       }
           $e2/$f9/              { LOOP    012D             }
           $73/$07/              { JNB     013D             }
           $83/$2c/$01/          { SUB Word Ptr [SI],1      }
           $46/                  { INC     SI               }
           $46/                  { INC     SI               }
           $eb/$f7/              { JMP     0134             }
           $1f);                 { POP     DS               }
    WITH answer DO BEGIN
      WHILE (len > 1) AND (dat[len] = 0) DO len := len - 1; END;
    END; (* sublong *)

  (* 1---------------1 *)

  PROCEDURE writepi(VAR pi : long);

    VAR
      l, l1, i, r, r1, x , nr : word;

    (* 2---------------2 *)

    PROCEDURE wr(c : char);

      BEGIN (* wr *)
      IF x MOD 6 = 0 THEN
        IF x < 69 THEN BEGIN
          write(' '); inc(x); END
        ELSE BEGIN
          IF nr MOD 5 = 0 THEN writeln;
          inc (nr); writeln;
          write('            '); x := 13; END;
      write(c); inc(x);
      END; (* wr *)

    (* 2---------------2 *)

    BEGIN (* writepi *)
    nr := 1;
{$ifdef timing}
    writeln(^g); exit;
{$endif}
    WITH pi DO BEGIN
      l := len; l1 := l - 1; i := 1;
      write('   Pi  =  ', dat[l], '.'); x := 13;
      FOR i := 1 TO number DIV 4 DO BEGIN
   (*   IF dat[i] = 0 THEN i := i + 1; *)
        dat[l] := 0; len := l1;
        mullong(pi, 10000, pi); r := dat[l];
        r1 := r DIV 100; r := r MOD 100;
        wr(chr(r1 DIV 10 + 48)); wr(chr(r1 MOD 10 + 48));
        wr(chr(r DIV 10 + 48));  wr(chr(r MOD 10 + 48)); END;
      END;
    writeln;
    END; (* writepi *)

  (* 1---------------1 *)


  PROCEDURE zerolong(VAR l : long; size : word);

    BEGIN (* zerolong *)
    fillchar(l.dat, size*2, #0); l.len := words;
    END; (* zerolong *)

  (* 1---------------1 *)

  PROCEDURE copylong(VAR fra, til : long);

    BEGIN (* copylong *)
    move(fra, til, words * 2 + 2);
    END; (* copylong *)

  (* 1---------------1 *)

  PROCEDURE getlong(VAR p : longptr; size : word);

    VAR
      d              : ^byte;
      bytes          : word;

    BEGIN (* getlong *)
    bytes := size + size + 2;
    REPEAT
      getmem(p, bytes);
      IF ofs(p^) = 0 THEN BEGIN
        zerolong(p^, size); exit; END;
      system.freemem(p, bytes); new(d);
    UNTIL false;
    END; (* getlong *)

  (* 1---------------1 *)

  VAR
    temp           : longptr;

  PROCEDURE writelong(VAR l : long; size : word);

    BEGIN (* writelong *)
    copylong(l, temp^); temp^.len := size;
    writepi(temp^);
    END; (* writelong *)

  (* 1---------------1 *)


  PROCEDURE arccotan(n : word; VAR angle : long);

    VAR
      n2, del,
      remainder      : word;
      positive       : boolean;

    BEGIN (* arccotoan *)
    zerolong(angle, words);
    zerolong(nin^, words);
    zerolong(link^, words);

    angle.dat[angle.len] := 1;
    divlong(angle, n, angle, remainder);

    n2 := n*n; del := 1; positive := true;

    copylong(angle, nin^);

    REPEAT
      divlong(nin^, n2, nin^, remainder);
      del := del + 2; positive := NOT positive;
      divlong(nin^, del, link^, remainder);
      IF positive THEN addlong(angle, link^)
      ELSE sublong(angle, link^);
    UNTIL (link^.len <= 1) AND (link^.dat[1] = 0);
    END; (* arccotan *)

  (* 1---------------1 *)

  BEGIN (* calcpi *)
  writeln; val(paramstr(1), number, i);
  writeln('Evaluate Pi (3.1415926...) with large precision.');
  IF (i > 0) OR (paramcount < 1) THEN BEGIN
    writeln('Alternate usage: CALCPI nn     (nn is digits to calculate)');
    write('Digits (max ', n_max: 0: 0, ' : ');
    readln(number); END;
  number := round(number / 4) * 4;
  words := round(number / w_dig + 2);
  gettime(tid1);
  getlong(pi, words+2);
  getlong(angle239, words+2);
  getlong(link, words+2);
  getlong(nin, words+2);
  getlong(temp, words+2);

  arccotan(5, pi^);                      { ATan(1/5) }
  addlong(pi^, pi^); addlong(pi^, pi^);  { * 4       }

  arccotan(239, angle239^);
  sublong(pi^, angle239^);
  addlong(pi^, pi^); addlong(pi^, pi^);  { * 4       }
  writeln; writepi(pi^); writeln;
  gettime(tid2);
  writeln(calctime(tid1, tid2) : 6 : 1, ' Seconds to calculate ',
          number, ' Decimals of pi');
  END. (* calcpi *)
.å