C$PROG IOFERR
      SUBROUTINE IOFERR(IOS)
C
      COMMON/LLL/ MSSG(28),NAMPROG(2),LOGUT,LOGUP,LISFLG,MSGF
      EQUIVALENCE (CMSER,MSER)
C
      INTEGER*4 MSER(20)
      CHARACTER*80 CMSER
C
      IF(IOS.EQ.0) RETURN
C
      CALL GET_DECF77ERR(IOS,CMSER)
C
      DO 10 I=1,20
      MSSG(I)=MSER(I)
   10 CONTINUE
      CALL MESSLOG(LOGUT,LOGUP)
      RETURN
      END
