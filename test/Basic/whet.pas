PROGRAM WHETSTONE;

(*
   To run the Whetstone benchmark, first choose how many million 
   Whetstones to perform and set the CONST IM. Larger values for IM
   will give greater precision and require more patience. 

   Run Whetstone and time how long it takes. The results are reported
   as Whetstones / second. If IM = 1.0 and it takes 2.0 seconds to run,
   then report 500,000 whetstones per second.

   The output from Whetstone should look like:


      0     0     0      1.000      -1.000      -1.000      -1.000
    120   140   120      -.068       -.463       -.730      -1.124
    140   120   120      -.055       -.447       -.711      -1.103
   3450     1     1      1.000      -1.000      -1.000      -1.000
   2100     1     2      6.000       6.000       -.711      -1.103
    320     1     2       .490        .490        .490        .490
   8990     1     2      1.000       1.000       1.000       1.000
   6160     1     2      3.000       2.000       3.000      -1.103
      0     2     3      1.000      -1.000      -1.000      -1.000
    930     2     3       .835        .835        .835        .835
END OF WHETSTONE,  1 Million Whetstones Performed


*)


CONST 

      IM = 1.0;        (* How many million whetstones to perform *)
      T=0.499975;
      T1=0.50025;
      T2=2.0;

TYPE
   ARGARRAY = ARRAY[1..4] OF REAL;

VAR E1 : ARGARRAY;
    X,Y,Z,X1,X2,X3,X4 : REAL;
	I, 
	J,
	K,
	L,
	N1,N2,N3,N4,N5,N6,N7,N8,N9,N10,N11 : INTEGER;

{$R-}               {disable range checking}

PROCEDURE PA(VAR E:ARGARRAY);

VAR J : INTEGER;

BEGIN
   J:=0;

   repeat
   E[1]:=(E[1]+E[2]+E[3]-E[4])*T;
   E[2]:=(E[1]+E[2]-E[3]+E[4])*T;
   E[3]:=(E[1]-E[2]+E[3]+E[4])*T;
   E[4]:=(-E[1]+E[2]+E[3]+E[4])/T2;
   J:=J+1;
   until J>=6;
END;  (* PROCEDURE PA*)


PROCEDURE P0;

BEGIN
   E1[J]:=E1[K];
   E1[K]:=E1[L];
   E1[L]:=E1[J]
END;   (* PROCEDURE P0 *)

PROCEDURE P3(X,Y:REAL;VAR Z:REAL);

BEGIN
   X:=T*(X+Y);
   Y:=T*(X+Y);
   Z:=(X+Y)/T2
END;  (* PROCEDURE P3 *)

PROCEDURE MODULE1; (* MODULE 1: SIMPLE IDENTIFIERS *)
VAR
	I : INTEGER;

BEGIN
  X1:=1.0;
  X2:=-1.0; X3:=-1.0; X4:=-1.0;

  FOR I:=1 TO N1 DO
  BEGIN
    X1:=(X1+X2+X3-X4)*T;
    X2:=(X1+X2-X3+X4)*T;
    X3:=(X1-X2+X3+X4)*T;
    X4:=(-X1+X2+X3+X4)*T
  END;

END; (* MODULE 1 *)

PROCEDURE MODULE2; (* MODULE 2: ARRAY ELEMENTS *)

VAR
	I : INTEGER;

BEGIN
E1[1]:=1.0;
E1[2]:=-1.0; E1[3]:=-1.0; E1[4]:=-1.0;

FOR I:=1 TO N2 DO
  BEGIN
  E1[1]:=(E1[1]+E1[2]+E1[3]-E1[4])*T;
  E1[2]:=(E1[1]+E1[2]-E1[3]+E1[4])*T;
  E1[3]:=(E1[1]-E1[2]+E1[3]+E1[4])*T;
  E1[4]:=(-E1[1]+E1[2]+E1[3]+E1[4])*T
  END;
END;  (* MODULE 2 *)

PROCEDURE MODULE4; (* MODULE 4: CONDITIONAL JUMPS *)

VAR
	I : INTEGER;

BEGIN
J:=1;
FOR I:=1 TO N4 DO
  BEGIN
    IF J=1 THEN
      J:=2
    ELSE
      J:=3;
    IF J>1 THEN
      J:=0
    ELSE
      J:=1;
    IF J<2 THEN
      J:=1
    ELSE
      J:=0
  END;
END; (* MODULE 4 *)

PROCEDURE MODULE6; (* INTEGER ARITHMETIC *)

VAR
	I : INTEGER;

BEGIN
  J:=1;
  K:=2;
  L:=3;

  FOR I:= 1 TO N6 DO
  BEGIN
    J:=J*(K-J)*(L-K);
    K:=L*K-(L-J)*K;
    L:=(L-K)*K+J;
    E1[L-1]:=J+K+L;
    E1[K-1]:=J*K*L
  END;
END; (* MODULE 6 *)

PROCEDURE MODULE7; (* MODULE 7: TRIG FUNCTIONS *)

VAR I    : INTEGER;
    TEMP : REAL;

BEGIN
X:=0.5; Y:=0.5;
FOR I:=1 TO N7 DO
  BEGIN
    TEMP:=COS(X+Y)+COS(X-Y)-1.0;
    X:=T*ARCTAN(T2*SIN(X)*COS(X)/TEMP);  
    TEMP:=COS(X+Y)+COS(X-Y)-1.0;
    Y:=T*ARCTAN(T2*SIN(Y)*COS(Y)/TEMP);
  END;
END; (* MODULE 7 *)

PROCEDURE MODULE8; (* MODULE 8: PROCEDURE CALLS *)
VAR
	I  : INTEGER;

BEGIN
  X:=1.0; Y:=1.0; Z:=1.0;

  FOR I:=1 TO N8 DO
    P3(X,Y,Z)
END; (* MODULE 8 *)

PROCEDURE MODULE10; (* MODULE 10: INTEGER ARTIHMETIC *)

VAR
	I  : INTEGER;

BEGIN
  J:=2;
  K:=3;
  FOR I:=1 TO N10 DO
    BEGIN
    J:=J+K;
    K:=J+K;
    J:=K-J;
    K:=K-J-J
  END;
END; (* MODULE 10 *)

PROCEDURE MODULE11; (* MODULE 11: STANDARD FUNCTIONS *)

VAR
	I  : INTEGER;

BEGIN
  X:=0.75;

  FOR I:=1 TO N11 DO
    X:=SQRT(EXP(LN(X)/T1));
END; (* MODULE 11 *)

PROCEDURE POUT(VAR N,J,K:INTEGER; VAR X1,X2,X3,X4:REAL);

BEGIN
  WRITE(N:7,J:6,K:6);
  WRITELN(X1:11:3,X2:12:3,X3:12:3,X4:12:3);
END;   (* PROCEDURE POUT *)

BEGIN  (* START WHETSTONE *)

  I := TRUNC( 10.0 * IM);
  N1:=0;
  N2:=12*I;
  N3:=14*I;
  N4:=345*I;
  N5:=0;
  N6:=210*I;
  N7:=32*I;
  N8:=899*I;
  N9:=616*I;
  N10:=0;
  N11:=93*I;

(* MODULAR PROGRAMMING IS USED TO REDUCE THE LENGTH OF MAIN CODE *)

MODULE1; (* SIMPLE IDENTIFIERS *)
POUT(N1,N1,N1,X1,X2,X3,X4);

MODULE2; (* ARRAY ELEMENTS *)
POUT(N2,N3,N2,E1[1],E1[2],E1[3],E1[4]);

(* MODULE 3: ARRAY AS A PARAMETER *)

FOR I:= 1 TO N3 DO

  PA(E1);
POUT(N3,N2,N2,E1[1],E1[2],E1[3],E1[4]);

(* END OF MODULE 3 *)

MODULE4; (* CONDITIONAL JUMPS *)
POUT(N4,J,J,X1,X2,X3,X4);

MODULE6; (* INTEGER ARITHMETIC *)
POUT(N6,J,K,E1[1],E1[2],E1[3],E1[4]);

MODULE7; (* TRIG FUNCTIONS *)
POUT(N7,J,K,X,X,Y,Y);

MODULE8; (* PROCEDURE CALLS *)
POUT(N8,J,K,X,Y,Z,Z);

(* MODULE 9: ARRAY REFERENCES *)

  J:=1;
  K:=2;
  L:=3;
  E1[1]:=1.0;
  E1[2]:=2.0;
  E1[3]:=3.0;

  FOR I:=1 TO N9 DO P0;

POUT(N9,J,K,E1[1],E1[2],E1[3],E1[4]);

MODULE10; (* INTEGER ARITHMETIC *)
POUT(N10,J,K,X1,X2,X3,X4);

MODULE11; (* STANDARD FUNCTIONS *)
POUT(N11,J,K,X,X,X,X);

WRITELN('END OF WHETSTONE, ', TRUNC(IM * 10.0)/10.0:4:1, 
        ' Million Whetstones Performed');
END. (* END WHETSTONE *)
