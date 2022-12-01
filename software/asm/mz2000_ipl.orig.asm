            ;****************************************************************
            ;
            ; MZ-2000 Initial Program Loader Firmware.
            ;
            ; Disassembled with dz80 v2.1 and comments copied from the
            ; MZ-80B IPL disassembly.
            ;
            ;****************************************************************
            ;
CR          EQU     0D8H
TR          EQU     0D9H
SCR         EQU     0DAH
DR          EQU     0DBH
DM          EQU     0DCH
HS          EQU     0DDH
PPIA        EQU     0E0H
PPIB        EQU     0E1H
PPIC        EQU     0E2H
PPICTL      EQU     0E3H
PIOA        EQU     0E8H
PIOCTLA     EQU     0E9H
PIOB        EQU     0EAH
PIOCTLB     EQU     0EBH
GRPHCTL     EQU     0F4H
IBUFE       EQU     0CF00H
ATRB        EQU     0CF00H
NAME        EQU     0CF01H
SIZE        EQU     0CF12H
DTADR       EQU     0CF14H
SUMDT       EQU     0FFE0H
TMCNT       EQU     0FFE2H
IBADR1      EQU     0CF00H
IBADR2      EQU     08000H
NTRACK      EQU     0FFE0H
NSECT       EQU     0FFE1H
BSIZE       EQU     0FFE2H
STTR        EQU     0FFE4H
STSE        EQU     0FFE5H
MTFG        EQU     0FFE6H
CLBF0       EQU     0FFE7H
CLBF1       EQU     0FFE8H
CLBF2       EQU     0FFE9H
CLBF3       EQU     0FFEAH
RETRY       EQU     0FFEBH
DRINO       EQU     0FFECH
PRGSTART    EQU     00000H

            ORG     PRGSTART

L0000:      JR      START               
            ;
            ; NST RESET
            ;
NST:        LD      A,003H              ;Set PC1 NST=1
            OUT     (PPICTL),A                                                                                      
START:      LD      A,082H              ;8255 A=OUT B=IN C=OUT
            OUT     (PPICTL),A                                                                                      
            LD      A,058H              ;PIO A=OUT
            OUT     (PPIC),A                                                                                        
            LD      SP,SUMDT            ;PIO B=IN
            LD      A,0F7H                                                                                          
            OUT     (PPIA),A                                                                                        
            LD      A,00FH                                                                                          
            OUT     (PIOCTLA),A         ;BST=1 NST=0 OPEN=1 WRITE=1
            LD      A,0CFH                                                                                          
            OUT     (PIOCTLB),A                                                                                     
            LD      A,0FFH                                                                                          
            OUT     (PIOCTLB),A                                                                                     
            XOR     A                   ;Set Graphics VRAM to default, input to GRPH I, no output.
            OUT     (0F6H),A                                                                                        
            OUT     (GRPHCTL),A                                                                                     
            INC     A                                                                                               
            OUT     (0F7H),A                                                                                        
            LD      A,007H              
            OUT     (0F5H),A            
            LD      A,0D3H              
            OUT     (PIOA),A            
            LD      HL,0D000H           
            LD      A,CR                
CLEAR:      LD      (HL),000H           ; DISPLAY CLEAR
            INC     HL                                                                                              
            CP      H                                                                                               
            JR      NZ,CLEAR            
            LD      A,0FFH                                                                                          
            OUT     (PPIA),A                                                                                        
            LD      A,003H                                                                                          
            OUT     (0F7H),A                                                                                        
            CALL    L006E                                                                                           
            LD      A,002H              
            OUT     (0F7H),A                                                                                        
            CALL    L006E               
            LD      A,001H              
            OUT     (0F7H),A            
            CALL    L006E               
            LD      A,013H              
            OUT     (PIOA),A            
            XOR     A                   
            LD      (DRINO),A           
            LD      (MTFG),A            
KEYIN:      CALL    KEYS1               
            BIT     3,A                 ;C - Cassette.
            JR      Z,CMT                                                                                           
            BIT     0,A                 ;/ - Boot external rom.
            JP      Z,EXROMT                                                                                        
            JR      NKIN                ;No selection, so standard startup, try FDC then CMT.
                                                                                                                    
L006E:      LD      HL,0C000H           
            DI                          
            IN      A,(PIOA)            
            SET     7,A                 
            RES     6,A                 
            OUT     (PIOA),A            
            LD      DE,0C001H
            LD      (HL),000H
            LD      BC,03E7FH
L0082:      LDIR    
            RES     7,A
            SET     6,A
            OUT     (PIOA),A
            EI      
            RET     

KEYS1:      LD      B,014H              ;Preserve A4-A7, set A4 to prevent all strobes low, the select line 5 (0-4).
KEYS:       IN      A,(PIOA)                                                                                        
            AND     0F0H                                                                                            
            OR      B                                                                                               
            OUT     (PIOA),A                                                                                        
            IN      A,(PIOB)            ;Read the strobed key.
            RET     

NKIN:       CALL    FDCC
            JP      Z,FD
            JR      CMT                 

FDCC:       LD      A,0A5H
            LD      B,A
            OUT     (TR),A
            CALL    DLY80U
            IN      A,(TR)
            CP      B
            RET     
            ;
            ;                                       ;
            ;  CMT CONTROL                          ;
            ;                                       ;
            ;
CMT:        CALL    MSTOP
            CALL    KYEMES
            CALL    RDINF
            JR      C,ST1               
            CALL    LDMSG
            LD      HL,NAME
            LD      E,010H
            LD      C,010H
            CALL    DISP2
            LD      A,(IBUFE)
            CP      001H
            JR      NZ,MISMCH           
            CALL    RDDAT
ST1:        PUSH    AF
            CALL    FR
            POP     AF
            JP      C,TRYAG
            JP      NST

MISMCH:     LD      HL,MES16
            LD      E,00AH
            LD      C,00FH
            CALL    DISP
            CALL    MSTOP
            SCF     
            JR      ST1                 
            ;
            ;READ INFORMATION
            ;      CF=1:ERROR
?RDI:
RDINF:      DI      
            IN      A,(PPIC)
            SET     5,A
            OUT     (PPIC),A
            LD      D,004H
            LD      BC,0080H
            LD      HL,IBUFE
RD1:        CALL    MOTOR
            JR      C,STPEIR            
            CALL    TMARK
            JR      C,STPEIR            
            CALL    RTAPE
            JR      C,STPEIR            
            BIT     3,D
            JR      Z,EIRTN             
STPEIR:     CALL    MSTOP
EIRTN:      EI      
            RET     
            ;
            ;
            ;READ DATA
            ;
?RDD:
RDDAT:      DI      
            LD      D,008H
            LD      BC,(0CF12H)
            LD      HL,IBADR2
            JR      RD1                 
            ;
            ;
            ;READ TAPE
            ;      BC=SIZE
            ;      DE=LOAD ADDRSS
RTAPE:      PUSH    DE
            PUSH    BC
            PUSH    HL
            LD      H,002H
RTP2:       CALL    SPDIN
            JR      C,TRTN1             ; BREAK
            JR      Z,RTP2              
            LD      D,H
            LD      HL,L0000
            LD      (SUMDT),HL
            POP     HL
            POP     BC
            PUSH    BC
            PUSH    HL
RTP3:       CALL    RBYTE
            JR      C,TRTN1             
            LD      (HL),A
            INC     HL
            DEC     BC
            LD      A,B
            OR      C
            JR      NZ,RTP3             
            LD      HL,(SUMDT)
            CALL    RBYTE
            JR      C,TRTN1             
            LD      E,A
            CALL    RBYTE
            JR      C,TRTN1             
            CP      L
            JR      NZ,RTP5             
            LD      A,E
            CP      H
            JR      Z,TRTN1             
RTP5:       DEC     D
            JR      Z,RTP6              
            LD      H,D
            JR      RTP2                

RTP6:       CALL    BOOTER
            SCF     
TRTN1:      POP     HL
            POP     BC
            POP     DE
            RET     

EDGE:       IN      A,(PPIB)
            CPL     
            RLCA    
            RET     C                   ; BREAK
            RLCA    
            JR      NC,EDGE             ; WAIT ON LOW
L016A:      IN      A,(PPIB)
            CPL     
            RLCA    
            RET     C                   ; BREAK
            RLCA    
            JR      C,L016A             ; WAIT ON HIGH
            RET     
            ; 1 BYTE READ
            ;      DATA=A
            ;      SUMDT STORE
RBYTE:      PUSH    HL
            LD      HL,00800H           ; 8 BITS
RBY1:       CALL    SPDIN
            JR      C,RBY3              ; BREAK
            JR      Z,RBY2              ; BIT=0
            PUSH    HL
            LD      HL,(SUMDT)          ;CHECKSUM
            INC     HL
            LD      (SUMDT),HL
            POP     HL
            SCF     
RBY2:       RL      L
            DEC     H
            JR      NZ,RBY1             
            CALL    EDGE
            LD      A,L
RBY3:       POP     HL
            RET     
            ;TAPE MARK DETECT
            ;      E=L:INFORMATION
            ;      E=S:DATA
TMARK:      PUSH    HL
            LD      HL,01414H
            BIT     3,D
            JR      NZ,TM0              
            ADD     HL,HL
TM0:        LD      (TMCNT),HL
TM1:        LD      HL,(TMCNT)
TM2:        CALL    SPDIN
            JR      C,RBY3              
            JR      Z,TM1               
            DEC     H
            JR      NZ,TM2              
TM3:        CALL    SPDIN
            JR      C,RBY3              
            JR      NZ,TM1              
            DEC     L
            JR      NZ,TM3              
            CALL    EDGE
            JR      RBY3                

SPDIN:      CALL    EDGE                ;WAIT ON HIGH
            RET     C                   ;BREAK
            CALL    DLY2
            IN      A,(PPIB)            ;READ BIT
            AND     040H
            RET     
            ;
            ;
            ;MOTOR ON
MOTOR:      PUSH    DE
            PUSH    BC
            PUSH    HL
            IN      A,(PPIB)
            AND     020H
            JR      Z,MOTRD             
            LD      HL,MES6
            LD      E,00AH
            LD      C,00EH
            CALL    DISP
            CALL    OPEN
MOT1:       IN      A,(PIOB)
            CPL     
            RLCA    
            JR      C,MOTR              
            IN      A,(PPIB)
            AND     020H
            JR      NZ,MOT1             
            CALL    KYEMES
            CALL    DEL1M
MOTRD:      CALL    PLAY
MOTR:       POP     HL
            POP     BC
            POP     DE
            RET     
            ;
            ;
            ;MOTOR STOP
MSTOP:      LD      A,0F7H
            OUT     (PPIA),A
            CALL    DEL6
            LD      A,0FFH
            OUT     (PPIA),A
            RET     
            ;EJECT
OPEN:       LD      A,008H              ;Reset PC4 - EJECT activate
            OUT     (PPICTL),A
            CALL    DEL6
            LD      A,009H
            OUT     (PPICTL),A          ;Set PC4 - Deactivate EJECT
            RET     

KYEMES:     LD      HL,MES3
            LD      E,004H
            LD      C,01CH
            CALL    DISP
            RET     
            ;
            ;PLAY
PLAY:       LD      A,0FBH
            OUT     (PPIA),A
            CALL    DEL6
            LD      A,0FFH
            OUT     (PPIA),A
            RET     

FR:         LD      A,0FEH
FR1:        OUT     (PPIA),A
            CALL    DEL6
            LD      A,0FFH
            OUT     (PPIA),A
            IN      A,(PPIC)
            RES     5,A
            OUT     (PPIC),A
            RET     
            ;
            ;TIMING DEL
DM1:        PUSH    AF
DM1A:       XOR     A
DM1B:       DEC     A
            JR      NZ,DM1B             
            DEC     BC
            LD      A,B
            OR      C
            JR      NZ,DM1A             
            POP     AF
            POP     BC
            RET     

DEL6:       PUSH    BC
            LD      BC,RDINF
            JR      DM1                 

DEL1M:      PUSH    BC
            LD      BC,060FH
            JR      DM1                 
            ;
            ;TAPE DELAY TIMING
            ;
            ;
DLY2:       LD      A,031H
DLY2A:      DEC     A
            JP      NZ,DLY2A
            RET     

LDMSG:      LD      HL,MES1
            LD      E,000H
            LD      C,00EH
            JR      DISP                

DISP2:      LD      A,0D3H
            OUT     (PIOA),A
            JR      DISP1               

BOOTER:     LD      HL,MES8
            LD      E,00AH
            LD      C,00DH
DISP:       LD      A,0D3H
            OUT     (PIOA),A
            EXX     
            LD      HL,0D000H
            LD      A,CR
DISP3:      LD      (HL),000H
            INC     HL
            CP      H
            JR      NZ,DISP3            
            EXX     
DISP1:      XOR     A
            LD      B,A
            LD      D,0D0H
            LDIR    
            LD      A,013H
            OUT     (PIOA),A
            RET     

            ;
MES1:       DB      "IPL is loading"
MES3:       DB      "IPL is looking for a program"
MES6:       DB      "Make ready CMT"
MES8:       DB      "Loading error"
MES9:       DB      "Make ready FD"
MES10:      DB      "Press F or C"
MES11:      DB      "F:Floppy disk    "
MES12:      DB      "C:Cassette tape"
MES13:      DB      "Drive No? (1-4)"
MES14:      DB      "This disk is not master    "
MES15:      DB      "Pressing S key starts the CMT"
MES16:      DB      "File mode error"
            ;
IPLMC:      DB      01H
            DB      "IPLPRO"

            ;
            ;
            ;FD
FD:         LD      IX,IBUFE
            XOR     A
            LD      (0CF1EH),A
            LD      (0CF1FH),A
            LD      IY,SUMDT
            LD      HL,0100H
            LD      (IY+002H),L
            LD      (IY+003H),H
            CALL    BREAD               ;INFORMATION INPUT
            LD      HL,IBUFE            ;MASTER CHECK
            LD      DE,IPLMC
            LD      B,006H
MCHECK:     LD      C,(HL)
            LD      A,(DE)
            CP      C
            JP      NZ,NMASTE
            INC     HL
            INC     DE
            DJNZ    MCHECK              
            CALL    LDMSG
            LD      HL,0CF07H
            LD      E,010H
            LD      C,00AH
            CALL    DISP2
            LD      IX,IBADR2
            LD      HL,(0CF14H)
            LD      (IY+002H),L
            LD      (IY+003H),H
            CALL    BREAD
            CALL    MOFF
            JP      NST

NODISK:     LD      HL,MES9
            LD      E,00AH
            LD      C,00DH
            CALL    DISP
            JP      ERR1
            ;
            ; READY CHECK
            ;
READY:      LD      A,(MTFG)
            RRCA    
            CALL    NC,MTON
            LD      A,(DRINO)           ;DRIVE NO GET
            OR      084H
            OUT     (DM),A              ;DRIVE SELECT MOTON
            XOR     A
            CALL    DLY60M
            LD      HL,L0000
REDY0:      DEC     HL
            LD      A,H
            OR      L
            JR      Z,NODISK            
            RET     C
            RET     C
            CPL     
            RLCA    
            JR      C,REDY0             
            LD      A,(DRINO)
            LD      C,A
            LD      HL,CLBF0
            LD      B,000H
            ADD     HL,BC
            BIT     0,(HL)
            RET     NZ
            CALL    RCLB
            SET     0,(HL)
            RET     
            ;
            ; MOTOR ON
            ;
MTON:       LD      A,080H
            OUT     (DM),A
            LD      B,00AH              ;1SEC DELAY
MTD1:       LD      HL,03C19H
MTD2:       DEC     HL
            LD      A,L
            OR      H
            JR      NZ,MTD2             
            DJNZ    MTD1                
            LD      A,001H
            LD      (MTFG),A
            RET     
            ;
            ; Track SEEK
            ;
SEEK:       LD      A,01BH
            CPL     
            OUT     (CR),A
            CALL    BUSY
            CALL    DLY60M
            IN      A,(CR)
            CPL     
            AND     099H
            RET     
            ;
            ;MOTOR OFF
            ;
MOFF:       CALL    DLY1M
            XOR     A
            OUT     (DM),A
            LD      (CLBF0),A
            LD      (CLBF1),A
            LD      (CLBF2),A
            LD      (CLBF3),A
            LD      (MTFG),A
            RET     
            ;
            ; RECALIBRATION
            ;
RCLB:       PUSH    HL
            LD      A,00BH
            CPL     
            OUT     (CR),A
            CALL    BUSY
            CALL    DLY60M
            IN      A,(CR)
            CPL     
            AND     085H
            XOR     004H
            POP     HL
            RET     Z
            JP      ERR
            ;
            ; BUSY AND WAIT
            ;
BUSY:       PUSH    DE
            PUSH    HL
            CALL    DLY80U
            LD      E,007H
BUSY2:      LD      HL,L0000
BUSY0:      DEC     HL
            LD      A,H
            OR      L
            JR      Z,BUSY1             
            IN      A,(CR)
            CPL     
            RRCA    
            JR      C,BUSY0             
            POP     HL
            POP     DE
            RET     

BUSY1:      DEC     E
            JR      NZ,BUSY2            
            JP      ERR
            ;
            ; DATA CHECK
            ;
CONVRT:     LD      B,000H
            LD      DE,0010H
            LD      HL,(0CF1EH)
            XOR     A
TRANS:      SBC     HL,DE
            JR      C,TRANS1            
            INC     B
            JR      TRANS               

TRANS1:     ADD     HL,DE
            LD      H,B
            INC     L
            LD      (IY+004H),H
            LD      (IY+005H),L
DCHK:       LD      A,(DRINO)
            CP      004H
            JR      NC,DTCK1            
            LD      A,(IY+004H)
            CP      046H
            JR      NC,DTCK1            
            LD      A,(IY+005H)
            OR      A
            JR      Z,DTCK1             
            CP      011H
            JR      NC,DTCK1            
            LD      A,(IY+002H)
            OR      (IY+003H)
            RET     NZ
DTCK1:      JP      ERR
            ;
            ; SEQUENTIAL READ
            ;
BREAD:      DI      
            CALL    CONVRT
            LD      A,00AH
            LD      (RETRY),A
READ1:      CALL    READY
            LD      D,(IY+003H)
            LD      A,(IY+002H)
            OR      A
            JR      Z,RE0               
            INC     D
RE0:        LD      A,(IY+005H)
            LD      (IY+001H),A
            LD      A,(IY+004H)
            LD      (IY+000H),A
            PUSH    IX
            POP     HL
RE8:        SRL     A
            CPL     
            OUT     (DR),A
            JR      NC,RE1              
            LD      A,001H
            JR      RE2                 

RE1:        LD      A,000H
RE2:        CPL     
            OUT     (HS),A
            CALL    SEEK
            JR      NZ,REE              
            LD      C,DR
            LD      A,(IY+000H)
            SRL     A
            CPL     
            OUT     (TR),A
            LD      A,(IY+001H)
            CPL     
            OUT     (SCR),A
            EXX     
            LD      HL,RE3
            PUSH    HL
            EXX     
            LD      A,094H
            CPL     
            OUT     (CR),A
            CALL    WAIT
RE6:        LD      B,000H
RE4:        IN      A,(CR)
            RRCA    
            RET     C
            RRCA    
            JR      C,RE4               
            INI     
            JR      NZ,RE4              
            INC     (IY+001H)
            LD      A,(IY+001H)
            CP      011H
            JR      Z,RETS              
            DEC     D
            JR      NZ,RE6              
            JR      RE5                 

RETS:       DEC     D
RE5:        LD      A,CR
            CPL     
            OUT     (CR),A
            CALL    BUSY
RE3:        IN      A,(CR)
            CPL     
            AND     0FFH
            JR      NZ,REE              
            EXX     
            POP     HL
            EXX     
            LD      A,(IY+001H)
            CP      011H
            JR      NZ,REX              
            LD      A,001H
            LD      (IY+001H),A
            INC     (IY+000H)
REX:        LD      A,D
            OR      A
            JR      NZ,RE7              
            LD      A,080H
            OUT     (DM),A
            RET     

RE7:        LD      A,(IY+000H)
            JR      RE8                 

REE:        LD      A,(RETRY)
            DEC     A
            LD      (RETRY),A
            JR      Z,ERR               
            CALL    RCLB
            JP      READ1
            ;
            ; WAIT AND BUSY OFF
            ;
WAIT:       PUSH    DE
            PUSH    HL
            CALL    DLY80U
            LD      E,008H
WAIT2:      LD      HL,L0000
WAIT0:      DEC     HL
            LD      A,H
            OR      L
            JR      Z,WAIT1             
            IN      A,(CR)
            CPL     
            RRCA    
            JR      NC,WAIT0            
            POP     HL
            POP     DE
            RET     

WAIT1:      DEC     E
            JR      NZ,WAIT2            
            JR      ERR                 

NMASTE:     LD      HL,MES14
            LD      E,007H
            LD      C,01BH
            CALL    DISP
            JR      ERR1                
            ;
            ;                                                 ;
            ;   ERRROR OR BREAK                               ;
            ;                                                 ;
            ;
ERR:        CALL    BOOTER
ERR1:       CALL    MOFF
            LD      SP,SUMDT
TRYAG:      CALL    FDCC
            JR      NZ,TRYAG3           
            LD      HL,MES10
            LD      E,05AH
            LD      C,00CH
            CALL    DISP2
            LD      E,0ABH
            LD      C,011H
            CALL    DISP2
            LD      E,0D3H
            LD      C,00FH
            CALL    DISP2
TRYAG1:     CALL    KEYS1
            BIT     3,A
            JP      Z,CMT
            BIT     6,A
            JR      Z,DNO               
            JR      TRYAG1              

DNO:        LD      HL,MES13
            LD      E,00AH
            LD      C,00FH
            CALL    DISP
DNO10:      LD      D,012H
            CALL    DNO0
            JR      NC,DNO3             
            LD      D,018H
            CALL    DNO0
            JR      NC,DNO3             
            JR      DNO10               

DNO3:       LD      A,B
            LD      (DRINO),A
            JP      FD

TRYAG3:     LD      HL,MES15
            LD      E,054H
            LD      C,01DH
            CALL    DISP2
TRYAG4:     LD      B,006H
TRYAG5:     CALL    KEYS
            BIT     3,A
            JP      Z,CMT
            JR      TRYAG5              

DNO0:       IN      A,(PIOA)
            AND     0F0H
            OR      D
            OUT     (PIOA),A
            IN      A,(PIOB)
            LD      B,000H
            LD      C,004H
            RRCA    
DNO1:       RRCA    
            RET     NC
            INC     B
            DEC     C
            JR      NZ,DNO1             
            RET     
            ;
            ;  TIME DELAY (1M &60M &80U )
            ;
DLY80U:     PUSH    DE
            LD      DE,000DH
            JP      DLYT

DLY1M:      PUSH    DE
            LD      DE,L0082
            JP      DLYT

DLY60M:     PUSH    DE
            LD      DE,01A2CH
DLYT:       DEC     DE
            LD      A,E
            OR      D
            JR      NZ,DLYT             
            POP     DE
            RET     
            ;
            ;
            ;                                             ;
            ;   INTRAM EXROM                              ;
            ;                                             ;
            ;
EXROMT:     LD      HL,IBADR2
            LD      IX,EROM1
            JR      SEROMA              

EROM1:      IN      A,(0F9H)
            CP      000H
            JP      NZ,NKIN
            LD      IX,EROM2
ERMT1:      JR      SEROMA              

EROM2:      IN      A,(0F9H)
            LD      (HL),A
            INC     HL
            LD      A,L
            OR      H
            JR      NZ,ERMT1            
            OUT     (0F8H),A
            JP      NST

SEROMA:     LD      A,H
            OUT     (0F8H),A
            LD      A,L
            OUT     (0F9H),A
            LD      D,004H
SEROMD:     DEC     D
            JR      NZ,SEROMD           
            JP      (IX)

            ; Align to rom size.
            DS       0800H - 1 - ($ + 0800H - 1) % 0800H, 0FFh
