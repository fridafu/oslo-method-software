      
      FUNCTION YXFCT(M,N)                                               
C     COMPUTES LN(NFACT/MFACT)                                          
C                                                                       
C     A TABLE OF LOG(N FACTORIAL), N = 0 TO 81.                         
C                                                                       
      DIMENSION F(111),A(52),B(49),X(10)                                
      COMMON /FACTRL/ F                                                 
      EQUIVALENCE (F(1),A(1)), (F(53),B(1)), (F(102),X(1))              
      DATA A/0.00, 0.00                                                 
     &    ,6.9314718055995D-01,1.7917594692281D+00,3.1780538303479D+00  
     &    ,4.7874917427820D+00,6.5792512120101D+00,8.5251613610654D+00  
     &    ,1.0604602902745D+01,1.2801827480081D+01,1.5104412573076D+01  
     &    ,1.7502307845874D+01,1.9987214495662D+01,2.2552163853123D+01  
     &    ,2.5191221182739D+01,2.7899271383841D+01,3.0671860106081D+01  
     &    ,3.3505073450137D+01,3.6395445208033D+01,3.9339884187199D+01  
     &    ,4.2335616460753D+01,4.5380138898477D+01,4.8471181351835D+01  
     &    ,5.1606675567764D+01,5.4784729398112D+01,5.8003605222981D+01  
     &    ,6.1261701761002D+01,6.4557538627006D+01,6.7889743137182D+01  
     &    ,7.1257038967168D+01,7.4658236348830D+01,7.8092223553315D+01  
     &    ,8.1557959456115D+01,8.5054467017582D+01,8.8580827542198D+01  
     &    ,9.2136175603687D+01,9.5719694542143D+01,9.9330612454787D+01  
     &    ,1.0296819861451D+02,1.0663176026064D+02,1.1032063971476D+02  
     &    ,1.1403421178146D+02,1.1777188139975D+02,1.2153308151544D+02  
     &    ,1.2531727114936D+02,1.2912393363913D+02,1.3295257503562D+02  
     &    ,1.3680272263733D+02,1.4067392364823D+02,1.4456574394634D+02  
     &    ,1.4847776695177D+02,1.5240959258450D+02/                     
      DATA B/1.5636083630308D+02                                        
     &    ,1.6033112821663D+02,1.6432011226320D+02,1.6832744544843D+02  
     &    ,1.7235279713916D+02,1.7639584840700D+02,1.8045629141754D+02  
     &    ,1.8453382886145D+02,1.8862817342367D+02,1.9273904728784D+02  
     &    ,1.9686618167289D+02,2.0100931639928D+02,2.0516819948264D+02  
     &    ,2.0934258675254D+02,2.1353224149456D+02,2.1773693411395D+02  
     &    ,2.2195644181913D+02,2.2619054832373D+02,2.3043904356578D+02  
     &    ,2.3470172344282D+02,2.3897838956183D+02,2.4326884900298D+02  
     &    ,2.4757291409619D+02,2.5189040220972D+02,2.5622113555001D+02  
     &    ,2.6056494097186D+02,2.6492164979855D+02,2.6929109765102D+02  
     &    ,2.7367312428569D+02,2.7806757344037D+02,2.8247429268763D+02  
     &    ,2.8689313329543D+02,2.9132395009427D+02,2.9576660135076D+02  
     &    ,3.0022094864701D+02,3.0468685676567D+02,3.0916419358015D+02  
     &    ,3.1365282994988D+02,3.1815263962021D+02,3.2266349912673D+02  
     &    ,3.2718528770378D+02,3.3171788719693D+02,3.3626118197920D+02  
     &    ,3.4081505887080D+02,3.4537940706227D+02,3.4995411804077D+02  
     &    ,3.5453908551944D+02,3.5913420536958D+02,3.6373937555556D+02/ 
      DATA X/10*1.0D20/                                                 
      DATA LFMAX/100/                                                   
C                                                                       
      YXFCT=0.0                                                         
      NUMAX=M-N                                                         
      IF(NUMAX) 30,100,20                                               
   20 ICTRL=0                                                           
      L1=N                                                              
      L2=M                                                              
      GO TO 40                                                          
   30 ICTRL=1                                                           
      NUMAX=-NUMAX                                                      
      L1=M                                                              
      L2=N                                                              
   40 CONTINUE                                                          
      IF(L1.GT.LFMAX) GO TO 45                                          
      IF(L2.GT.LFMAX) GO TO 42                                          
      YXFCT=F(L2+1)-F(L1+1)                                             
      GO TO 90                                                          
   42 CONTINUE                                                          
      YXFCT=F(LFMAX+1)-F(L1+1)                                          
      L1=LFMAX                                                          
      NUMAX=L2-L1                                                       
   45 CONTINUE                                                          
      FCTOR=L1                                                          
      TERM=1.0                                                          
      DO 50 NU=1,NUMAX                                                  
      FCTOR=FCTOR+1.0                                                   
      TERM=TERM*FCTOR                                                   
      IF(TERM.LT.1.E20) GO TO 50                                        
      YXFCT=YXFCT+ALOG(TERM)                                            
      TERM=1.0                                                          
   50 CONTINUE                                                          
      YXFCT=YXFCT+ALOG(TERM)                                            
   90 CONTINUE                                                          
      IF(ICTRL.EQ.0) YXFCT=-YXFCT                                       
  100 RETURN                                                            
      END
