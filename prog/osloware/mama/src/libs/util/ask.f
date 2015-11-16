
C======================================================================

      SUBROUTINE ASK(MESAG,NCM,ANS,NCA)

C         mesag: question to be asked (integer array)
C         ncm:   number of characters in question (integer)
C         ans:   answer recieved (character*40)
C         nca:   number of characters received in answer (integer)
C         mca:   max. number of characters asked for in answer (integer)

C patched by Andreas Schiller Sep. 10, 2003
C Reason: more standard repetitive format

      CHARACTER*1   MESAG(80)
      INTEGER      NCM, NCA
C  grabchar is a C function which returns the ascii value of the pressed key, 
C        (this value is grabbed - the return key does not have to be hit)

      INTEGER   GRABCHAR
      EXTERNAL  GRABCHAR

      CHARACTER*40 ANS
      COMMON /LUS/ IR,IW,IP,IG

C new variable added here, A.S.
      CHARACTER*11 F1STR

      IMCA=40
      GO TO 10

           ENTRY ASK2(MESAG,NCM,ANS,NCA,MCA)

      IMCA=MCA

C         output prompt message....

C changed the next three lines, A.S.
      WRITE(F1STR,20)NCM
10    IF (NCM.GT.0) WRITE(IW,FMT=F1STR) (MESAG(I),I=1,NCM)
20    FORMAT('(',I5.5,'A1,$)')

C         read response....
      IF (IMCA.EQ.0) RETURN

      ANS=' '
      IF (IMCA.EQ.1) THEN
         JCHAR = GRABCHAR()
         ANS(1:1) = CHAR(JCHAR)
         NCA = 1
      ELSE
         READ(IR,'(A40)') ANS
         NCA = 40
      ENDIF

      IF (NCA.GT.IMCA) NCA=IMCA
      IF (NCA.LE.0) GO TO 70

C            replace <CR> or <LF> with a space....
      IF (ANS(NCA:NCA).EQ.CHAR(13) .OR.
     +    ANS(NCA:NCA).EQ.CHAR(10)) THEN
         ANS(NCA:NCA)=' '
         NCA=NCA-1
         IF (NCA.LE.0) GO TO 70
      ELSEIF (IMCA.EQ.1) THEN
C           since no <CR> or <LF> echoed, force new line....
         WRITE(IW,*) CHAR(13)
      ENDIF

C           remove trailing blanks....
      DO 60 I=NCA,1,-1
         IF (ANS(I:I).NE.' ') GO TO 70
         NCA=NCA-1
60    CONTINUE

70    RETURN

      END
