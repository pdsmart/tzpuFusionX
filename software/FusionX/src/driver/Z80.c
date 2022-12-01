/* Z80 v0.2
       ______  ______ ______
      /\___  \/\  __ \\  __ \
 ____ \/__/  /\_\  __ \\ \/\ \ ________________________________________________
|        /\_____\\_____\\_____\                                                |
|  Zilog \/_____//_____//_____/ CPU Emulator                                   |
|  Copyright (C) 1999-2022 Manuel Sainz de Baranda y Goñi.                     |
|                                                                              |
|  This emulator is free software: you can redistribute it and/or modify it    |
|  under the terms of the GNU Lesser General Public License as published by    |
|  the Free Software Foundation, either version 3 of the License, or (at your  |
|  option) any later version.                                                  |
|                                                                              |
|  This emulator is distributed in the hope that it will be useful, but        |
|  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY  |
|  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public      |
|  License for more details.                                                   |
|                                                                              |
|  You should have received a copy of the GNU Lesser General Public License    |
|  along with this emulator. If not, see <http://www.gnu.org/licenses/>.       |
|                                                                              |
|  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --  |
|                                                                              |
|  A NOTE FROM THE ORIGINAL AUTHOR                                             |
|                                                                              |
|  Those familiar with the official documentation of the Zilog Z80 CPU will    |
|  find this source code quite intuitive. The purpose has not been to write    |
|  the fastest possible emulator, although the speed aspect is not neglected,  |
|                                          but a portable, hackable and well   |
|             .----._.----.                structured piece of software;       |
|   A11 <-01--|1         o|--40-> A10      something small, solid and elegant  |
|   A12 <-02--|           |--39-> A09      that can stand the test of time     |
|   A13 <-03--|           |--38-> A08      with no need for major changes.     |
|   A14 <-04--|           |--37-> A07                                          |
|   A15 <-05--|           |--36-> A06      Some of the main design decisions   |
|   CLK --06->|           |--35-> A05      have been the following:            |
|    D4 <-07->|           |--34-> A04                                          |
|    D3 <-08->|.---------.|--33-> A03      1. Opcode partial decoding keeps    |
|    D5 <-09->|| ZILOG   ||--32-> A02      the code small and maintainable.    |
|    D6 <-10->|| Z80     ||--31-> A01                                          |
|   +5V --11--|| CPU     ||--30-> A00      2. Function pointer tables for      |
|    D2 <-12->||         ||--29-- GND      opcode selection allow easy reuse   |
|    D7 <-13->|'---------'|--28-> RFSH     of almost all instruction code      |
|    D0 <-14->|           |--27-> M1       in the interrupt mode 0.            |
|    D1 <-15->|           |<-26-- RESET                                        |
|   INT --16->|           |<-25-- BUSREQ   3. Avoiding conditional statements  |
|   NMI --17->|           |<-24-- WAIT     as much as possible reduces the     |
|  HALT <-18--|           |--23-> BUSACK   branch penalty in modern pipelined  |
|  MREQ <-19--|           |--22-> WR       processors.                         |
|  IORQ <-20--|           |--21-> RD                                           |
|             '-----------'                                                    |
|    Zilog Z80 CPU, May 1976 version                                           |
|       40-pin ceramic DIP pinout                                      Manuel  |
|                                                                              |
'=============================================================================*/

#ifndef Z80_DEPENDENCIES_HEADER
#	include <Z/constants/pointer.h>
#	include <Z/macros/bitwise.h>
#endif

#ifdef Z80_STATIC
#	define Z80_API
#else
#	define Z80_API Z_API_EXPORT
#endif

#ifdef Z80_WITH_LOCAL_HEADER
#	include "Z80.h"
#else
#	include <Z80.h>
#endif


/* MARK: - Types */

typedef zuint8 (* Instruction)(Z80 *self);

#ifdef Z80_WITH_FULL_IM0
	typedef struct {
		Z80*	  z80;
		void*	  context;
		Z80Read	  fetch;
		Z80Read	  read;
		Z80Write  write;
		Z80Read	  in;
		Z80Write  out;
		Z80Notify ld_i_a;
		Z80Notify ld_r_a;
		Z80Notify reti;
		Z80Notify retn;
		zuint16	  pc;
	} IM0;
#endif


/* MARK: - External Macros */

#define REGISTER_OFFSET(member) (zusize)Z_MEMBER_OFFSET(Z80, member)
#define ROL(value)		Z_UINT8_ROTATE_LEFT (value, 1)
#define ROR(value)		Z_UINT8_ROTATE_RIGHT(value, 1)


/* MARK: - Instance Variable and Callback Shortcuts */

#define MEMPTR	  self->memptr.uint16_value
#define PC	  self->pc.uint16_value
#define SP	  self->sp.uint16_value
#define XY	  self->xy.uint16_value
#define IX	  self->ix_iy[0].uint16_value
#define IY	  self->ix_iy[1].uint16_value
#define AF	  self->af.uint16_value
#define BC	  self->bc.uint16_value
#define DE	  self->de.uint16_value
#define HL	  self->hl.uint16_value
#define AF_	  self->af_.uint16_value
#define BC_	  self->bc_.uint16_value
#define DE_	  self->de_.uint16_value
#define HL_	  self->hl_.uint16_value
#define MEMPTRH	  self->memptr.uint8_values.at_1
#define MEMPTRL	  self->memptr.uint8_values.at_0
#define PCH	  self->pc.uint8_values.at_1
#define A	  self->af.uint8_values.at_1
#define F	  self->af.uint8_values.at_0
#define B	  self->bc.uint8_values.at_1
#define C	  self->bc.uint8_values.at_0
#define E	  self->de.uint8_values.at_0
#define L	  self->hl.uint8_values.at_0
#define I	  self->i
#define R	  self->r
#define R7	  self->r7
#define Q	  self->q
#define IFF1	  self->iff1
#define IFF2	  self->iff2
#define IM	  self->im
#define HALT_LINE self->halt_line
#define INT_LINE  self->int_line
#define DATA	  self->data.uint8_array
#define REQUEST	  self->request
#define RESUME	  self->resume
#define OPTIONS	  self->options
#define CONTEXT	  self->context

#define FETCH_OPCODE(address) self->fetch_opcode(CONTEXT, address)
#define FETCH(address)	      self->fetch(CONTEXT, address)
#define READ(address)	      self->read (CONTEXT, address)
#define WRITE(address, value) self->write(CONTEXT, address, value)
#define IN(port)	      self->in (CONTEXT, port)
#define OUT(port, value)      self->out(CONTEXT, port, value)
#define NOTIFY(callback)      if (self->callback != Z_NULL) self->callback(CONTEXT)
#define INTA		      self->inta(CONTEXT, PC)


/* MARK: - Callbacks: 16-bit Operations */

static Z_INLINE zuint16 fetch_16(Z80 const *self, zuint16 address)
	{
	zuint8 t = FETCH(address);
	return (zuint16)(t | ((zuint16)FETCH(address + 1) << 8));
	}


static Z_INLINE zuint16 read_16(Z80 const *self, zuint16 address)
	{
	zuint8 t = READ(address);
	return (zuint16)(t | ((zuint16)READ(address + 1) << 8));
	}


static Z_INLINE void write_16f(Z80 const *self, zuint16 address, zuint16 value)
	{
	WRITE(address, (zuint8)value);
	WRITE(address + 1, (zuint8)(value >> 8));
	}


static Z_INLINE void write_16b(Z80 const *self, zuint16 address, zuint16 value)
	{
	WRITE(address + 1, (zuint8)(value >> 8));
	WRITE(address, (zuint8)value);
	}


#ifndef Z80_WITH_FULL_IM0
	static Z_INLINE zuint16 int_fetch_16(Z80 const *self)
		{
		zuint8 t = self->int_fetch(CONTEXT, PC);
		return (zuint16)(t | ((zuint16)self->int_fetch(CONTEXT, PC) << 8));
		}
#endif


#define FETCH_16(address)	  fetch_16 (self, address)
#define READ_16(address)	  read_16  (self, address)
#define WRITE_16F(address, value) write_16f(self, address, value)
#define WRITE_16B(address, value) write_16b(self, address, value)


/* MARK: - Flags */

/*----------------.
| 7 6 5 4 3 2 1 0 |
| S Z Y H X P N C |
'-|-|-|-|-|-|-|-|-'
  | | | | | | | '-- carry / borrow
  | | | | | | '---- addition / subtraction
  | | | | | '------ parity (P) / two's complement signed overflow (V)
  | | | | '-------- result's bit 3 (undocumented)
  | | | '---------- half carry / half borrow
  | | '------------ result's bit 5 (undocumented)
  | '-------------- zero
  '---------------- sign */

#define SF 128
#define ZF  64
#define YF  32
#define HF  16
#define XF   8
#define PF   4
#define NF   2
#define CF   1

#define SZPCF (SF | ZF | PF | CF)
#define SZPF  (SF | ZF | PF	)
#define SZCF  (SF | ZF | CF	)
#define SYXF  (SF | YF | XF	)
#define ZPF   (ZF | PF		)
#define YXCF  (YF | XF | CF	)
#define YXF   (YF | XF		)
#define HCF   (HF | CF		)

#define F_SZPC (F & SZPCF)
#define F_SZP  (F &  SZPF)
#define F_SZC  (F &  SZCF)
#define F_C    (F &    CF)
#define A_SYX  (A &  SYXF)
#define A_YX   (A &   YXF)

#define ZF_ZERO(value) (!(value) << 6)

#ifdef Z80_WITH_Q
#	define FLAGS Q = F
#	define Q_0   Q = 0;
#else
#	define FLAGS F
#	define Q_0
#endif


/* MARK: - P/V Flag Computation */

static zuint8 const pf_parity_table[256] = {
/*	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
/* 0 */ 4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
/* 1 */ 0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
/* 2 */ 0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
/* 3 */ 4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
/* 4 */ 0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
/* 5 */ 4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
/* 6 */ 4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
/* 7 */ 0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
/* 8 */ 0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
/* 9 */ 4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
/* A */ 4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
/* B */ 0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
/* C */ 4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
/* D */ 0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
/* E */ 0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
/* F */ 4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4
};

#define PF_PARITY(value) pf_parity_table[value]

#define PF_OVERFLOW(bits, result, lhs, rhs) \
	(((zuint##bits)((lhs ^ rhs) & (lhs ^ result)) >> (bits - 3)) & PF)


/* MARK: - 8-Bit Register Resolution */

/*---------.   .---------------------.
| 76543210 |   |  J / K  |   O / P   |
|----------|   |---------+-----------|
| __jjj___ |   | 000 = b | 000 = b   |
| _____kkk |   | 001 = c | 001 = c   |
| __ooo___ |   | 010 = d | 010 = d   |
| _____ppp |   | 011 = e | 011 = e   |
'----------'   | 100 = h | 100 = XYh |
	       | 101 = l | 101 = XYl |
	       | 111 = a | 111 = a   |
	       '--------------------*/

static zusize const j_k_table[8] = {
	REGISTER_OFFSET(bc.uint8_values.at_1),
	REGISTER_OFFSET(bc.uint8_values.at_0),
	REGISTER_OFFSET(de.uint8_values.at_1),
	REGISTER_OFFSET(de.uint8_values.at_0),
	REGISTER_OFFSET(hl.uint8_values.at_1),
	REGISTER_OFFSET(hl.uint8_values.at_0),
	0,
	REGISTER_OFFSET(af.uint8_values.at_1)
};

static zusize const o_p_table[8] = {
	REGISTER_OFFSET(bc.uint8_values.at_1),
	REGISTER_OFFSET(bc.uint8_values.at_0),
	REGISTER_OFFSET(de.uint8_values.at_1),
	REGISTER_OFFSET(de.uint8_values.at_0),
	REGISTER_OFFSET(xy.uint8_values.at_1),
	REGISTER_OFFSET(xy.uint8_values.at_0),
	0,
	REGISTER_OFFSET(af.uint8_values.at_1)
};

#define REGISTER_8(table, offset, shift) \
	*((zuint8 *)self + table[(DATA[offset] shift) & 7])

#define J0 REGISTER_8(j_k_table, 0, >> 3   )
#define J1 REGISTER_8(j_k_table, 1, >> 3   )
#define K0 REGISTER_8(j_k_table, 0, Z_EMPTY)
#define K1 REGISTER_8(j_k_table, 1, Z_EMPTY)
#define K3 REGISTER_8(j_k_table, 3, Z_EMPTY)
#define O  REGISTER_8(o_p_table, 1, >> 3   )
#define P  REGISTER_8(o_p_table, 1, Z_EMPTY)


/* MARK: - 16-Bit Register Resolution */

/*---------.   .-----------------------------.
| 76543210 |   |    S	 |    T    |	W    |
|----------|   |---------+---------+---------|
| __ss____ |   | 00 = bc | 00 = bc | 00 = bc |
| __tt____ |   | 01 = de | 01 = de | 01 = de |
| __ww____ |   | 10 = hl | 10 = hl | 10 = XY |
'----------'   | 11 = sp | 11 = af | 11 = sp |
	       '----------------------------*/

static zusize const s_table[4] = {
	REGISTER_OFFSET(bc.uint16_value),
	REGISTER_OFFSET(de.uint16_value),
	REGISTER_OFFSET(hl.uint16_value),
	REGISTER_OFFSET(sp)
};

static zusize const t_table[4] = {
	REGISTER_OFFSET(bc.uint16_value),
	REGISTER_OFFSET(de.uint16_value),
	REGISTER_OFFSET(hl.uint16_value),
	REGISTER_OFFSET(af.uint16_value)
};

static zusize const w_table[4] = {
	REGISTER_OFFSET(bc.uint16_value),
	REGISTER_OFFSET(de.uint16_value),
	REGISTER_OFFSET(xy.uint16_value),
	REGISTER_OFFSET(sp)
};

#define REGISTER_16(table, offset) \
	*(zuint16 *)(void *)((zchar *)self + table[(DATA[offset] >> 4) & 3])

#define SS0 REGISTER_16(s_table, 0)
#define SS1 REGISTER_16(s_table, 1)
#define TT  REGISTER_16(t_table, 0)
#define WW  REGISTER_16(w_table, 1)


/* MARK: - Condition Evaluation */

/*---------.   .----------.
| 76543210 |   |     Z	  |
|----------|   |----------|
| __zzz___ |   | 000 = nz |
| ___zz___ |   | 001 = z  |
'----------'   | 010 = nc |
	       | 011 = c  |
	       | 100 = po |
	       | 101 = pe |
	       | 110 = p  |
	       | 111 = m  |
	       '---------*/

static zuint8 const z_table[8] = {ZF, ZF, CF, CF, PF, PF, SF, SF};


static Z_INLINE zsint zzz(Z80 const *self, zuint8 mask)
	{
	zsint z = (DATA[0] >> 3) & mask;

	return !(F & z_table[z]) ^ (z & 1);
	}


/* MARK: - 8-Bit Arithmetic and Logical Operations */

/*---------.   .---------------------.
| 76543210 |   |     U	    SZYHXPNC |
|----------|   |---------------------|
| __uuu___ |   | 000 = add  szycxv0c |
| _____10v |   | 001 = adc  szycxv0c |
'----------'   | 010 = sub  szybxv1b |
	       | 011 = sbc  szybxv1b |
	       | 100 = and  szy1xp00 |
	       | 101 = xor  szy0xp00 |
	       | 110 = or   szy0xp00 |
	       | 111 = cp   sz*b*v1b |
	       |---------------------|
	       |     V	    SZYHXPNC |
	       |---------------------|
	       | 100 = inc  szycxv0. |
	       | 101 = dec  szybxv1. |
	       '--------------------*/

static void uuu(Z80 *self, zuint8 offset, zuint8 value)
	{
	zuint8 t, f;

	switch ((DATA[offset] >> 3) & 7)
		{
		case 0: /* add */
		t = A + value;

		f =	((zuint)A + value > 255)     | /* CF = carry	  */
			PF_OVERFLOW(8, t, A, ~value) | /* PF = overflow	  */
			((A ^ value ^ t) & HF);	       /* HF = half-carry */
		A = t;				       /* NF = 0	  */
		break;

		case 1: /* adc */
		t = A + value + (f = F_C);

		f =	((zuint)A + value + f > 255) | /* CF = carry	  */
			PF_OVERFLOW(8, t, A, ~value) | /* PF = overflow	  */
			((A ^ value ^ t) & HF);	       /* HF = half-carry */
						       /* NF = 0	  */
		A = t;
		break;

		case 2: /* sub */
		t = A - value;

		f =	(A < value)		    | /* CF = borrow	  */
			NF			    | /* NF = 1		  */
			PF_OVERFLOW(8, t, A, value) | /* PF = overflow	  */
			((A ^ value ^ t) & HF);	      /* HF = half-borrow */
		A = t;
		break;

		case 3: /* sbc */
		t = A - value - (f = F_C);

		f =	((zsint)A - value - f < 0)  | /* CF = borrow	  */
			NF			    | /* NF = 1		  */
			PF_OVERFLOW(8, t, A, value) | /* PF = overflow	  */
			((A ^ value ^ t) & HF);	      /* HF = half-borrow */

		A = t;
		break;

		case 4: /* and */
		A &= value;
		f = HF | PF_PARITY(A); /* HF = 1; PF = parity */
		break;		       /* NF, CF = 0	      */

		case 5: /* xor */
		A ^= value;
		f = PF_PARITY(A); /* PF = parity    */
		break;		  /* HF, NF, CF = 0 */

		case 6: /* or */
		A |= value;
		f = PF_PARITY(A); /* PF = parity    */
		break;		  /* HF, NF, CF = 0 */

		case 7: /* cp */
		t = A - value;

		FLAGS = (zuint8)(
			(t & SF)		    | /* SF = sign		*/
			ZF_ZERO(t)		    | /* ZF = zero		*/
			((A ^ value ^ t) & HF)	    | /* HF = half-borrow	*/
			PF_OVERFLOW(8, t, A, value) | /* PF = overflow		*/
			(A < value)		    | /* CF = borrow		*/
			(value & YXF)		    | /* YF = rhs.5; XF = rhs.3 */
			NF);			      /* NF = 1			*/

		return;
		}

	FLAGS = (zuint8)(
		f     |	     /* HF, PF, NF and CF already computed */
		A_SYX |	     /* SF = sign; YF = Y; XF = X	   */
		ZF_ZERO(A)); /* ZF = zero			   */
	}


static zuint8 vvv(Z80 *self, zuint8 offset, zuint8 value)
	{
	zuint8 t, pnf;

	/* dec */
	if (DATA[offset] & 1)
		{
		pnf = (zuint8)(((value == 128) << 2) | NF); /* PF = overflow; NF = 1 */
		t = value - 1;
		}

	/* inc */
	else	{
		pnf = (zuint8)((value == 127) << 2); /* PF = overflow; NF = 0 */
		t = value + 1;
		}

	FLAGS = (zuint8)(
		pnf		   | /* PF and NF already computed */
		(t & SYXF)	   | /* SF = sign; YF = Y; XF = X  */
		ZF_ZERO(t)	   | /* ZF = zero		   */
		((value ^ t) & HF) | /* HF = half-carry/borrow	   */
		F_C);		     /* CF unchanged		   */

	return t;
	}


/* MARK: - Rotation and Shift Operations */

/*---------.   .-----------.
| 76543210 |   |     G	   |
|----------|   |-----------|
| __ggg___ |   | 000 = rlc |
'----------'   | 001 = rrc |
	       | 010 = rl  |
	       | 011 = rr  |
	       | 100 = sla |
	       | 101 = sra |
	       | 110 = sll |
	       | 111 = srl |
	       '----------*/

static zuint8 ggg(Z80 *self, zuint8 offset, zuint8 value)
	{
	zuint8 cf;

	switch ((DATA[offset] >> 3) & 7)
		{
		/* rlc		 .----------------.
			.----.	 |  .---------.   |
			| CF |<-----| 7 <-- 0 |<--'
			'----'	    '--------*/
		case 0:
		cf = (value = ROL(value)) & 1;
		break;

		/* rrc	.----------------.
			|   .---------.  |   .----.
			'-->| 7 --> 0 |----->| CF |
			    '---------'      '---*/
		case 1:
		cf = value & 1;
		value = ROR(value);
		break;

		/* rl	.-------------------------.
			|  .----.   .---------.   |
			'--| CF |<--| 7 <-- 0 |<--'
			   '----'   '--------*/
		case 2:
		cf = value >> 7;
		value = (zuint8)((value << 1) | F_C);
		break;

		/* rr	.-------------------------.
			|   .---------.   .----.  |
			'-->| 7 --> 0 |-->| CF |--'
			    '---------'   '---*/
		case 3:
		cf = value & 1;
		value = (zuint8)((value >> 1) | ((zuint8)F_C << 7));
		break;

		/* sla	.----.	 .---------.
			| CF |<--| 7 <-- 0 |<-- 0
			'----'	 '--------*/
		case 4:
		cf = value >> 7;
		value <<= 1;
		break;

		/* sra	    .---------.   .----.
			.-->| 7 --> 0 |-->| CF |
			|   '---------'   '----'
			|     |
			'----*/
		case 5:
		cf = value & 1;
		value = (zuint8)((value & 128) | (value >> 1));
		break;

		/* sll	.----.	 .---------.
			| CF |<--| 7 <-- 0 |<-- 1
			'----'	 '--------*/
		case 6:
		cf = value >> 7;
		value = (zuint8)((value << 1) | 1);
		break;

		/* srl	     .---------.   .----.
			0 -->| 7 --> 0 |-->| CF |
			     '---------'   '---*/
		case 7:
		cf = value & 1;
		value >>= 1;
		break;

		/* Uncoment to avoid compiler warnings */
		/*default: cf = 0;*/
		}

	FLAGS = (zuint8)(
		(value & SYXF)	 | /* SF = sign; YF = Y; XF = X */
		ZF_ZERO(value)	 | /* ZF = zero			*/
		PF_PARITY(value) | /* PF = parity		*/
		cf);		   /* CF already computed	*/
				   /* HF, NF = 0		*/
	return value;
	}


/* MARK: - Bit Set and Reset Operations */

/*---------.   .---------.
| 76543210 |   |    M	 |
|----------|   |---------|
| _m______ |   | 0 = res |
'----------'   | 1 = set |
	       '--------*/

static Z_INLINE zuint8 m(Z80 *self, zuint8 offset, zuint8 value)
	{
	zuint8 t;

	Q_0
	t = DATA[offset];

	return (zuint8)((t & 64)
		? value |  (1U << ((t >> 3) & 7))
		: value & ~(1U << ((t >> 3) & 7)));
	}


/* MARK: - Function Shortcuts and Reusable Code */

#define N(index)	     ((DATA[index] >> 3) & 7)
#define Z(mask)		     zzz(self, mask)
#define U0(value)	     uuu(self, 0, value)
#define U1(value)	     uuu(self, 1, value)
#define V0(value)	     vvv(self, 0, value)
#define V1(value)	     vvv(self, 1, value)
#define G1(value)	     ggg(self, 1, value)
#define G3(value)	     ggg(self, 3, value)
#define M1(value)	     m	(self, 1, value)
#define M3(value)	     m	(self, 3, value)
#define PUSH(value)	     WRITE_16B(SP -= 2, value)
#define R_ALL		     ((R & 127) | (R7 & 128))
#define RET		     MEMPTR = PC = READ_16(SP); SP += 2
#define FETCH_XY_EA(address) MEMPTR = (zuint16)(XY + (zsint8)FETCH(address))
#define INSTRUCTION(name)    static zuint8 name(Z80 *self)
#define IS_XY_PREFIX(opcode) ((opcode) & 0xDF) == 0xDD
#define EX(a, b)	     t = a; a = b; b = t


#define LD_A_IR(rhs)					      \
	A = rhs;					      \
							      \
	FLAGS = (zuint8)(     /* HF, NF = 0		   */ \
		A_SYX	    | /* SF = sign; YF = Y; XF = X */ \
		ZF_ZERO(A)  | /* ZF = zero		   */ \
		(IFF2 << 2) | /* PF = IFF2		   */ \
		F_C);	      /* CF unchanged		   */ \
							      \
	PC += 2;					      \
	return 9


#define LDX(operator)					     \
	zuint8 t = READ(HL operator);			     \
							     \
	WRITE(DE operator, t);				     \
	t += A;						     \
							     \
	FLAGS = (zuint8)(	  /* HF, NF = 0		  */ \
		F_SZC	       |  /* SF, ZF, CF unchanged */ \
		((t & 2) << 4) |  /* YF = (A + [HLi]).1	  */ \
		(t & XF)       |  /* XF = (A + [HLi]).3	  */ \
		(!!(--BC) << 2)); /* PF = !!BCo		  */ \
							     \
	PC += 2;					     \
	return 16


#define LDXR(operator)							   \
	zuint8 t = READ(HL operator);					   \
									   \
	WRITE(DE operator, t);						   \
	t += A;								   \
									   \
	if (--BC)							   \
		{			    /* HF, NF = 0		*/ \
		FLAGS = F_SZC		  | /* SF, ZF, CF unchanged	*/ \
			((PC >> 8) & YXF) | /* YF = PCi.13; XF = PCi.11 */ \
			PF;		    /* PF = 1			*/ \
									   \
		MEMPTR = PC + 1;					   \
		return 21;						   \
		}							   \
									   \
	FLAGS = (zuint8)(	 /* HF, PF, NF = 0	 */		   \
		F_SZC	       | /* SF, ZF, CF unchanged */		   \
		((t & 2) << 4) | /* YF = (A + [HLi]).1	 */		   \
		(t & XF));	 /* XF = (A + [HLi]).3	 */		   \
									   \
	PC += 2;							   \
	return 16


#define CPX(operator)						 \
	zuint8 n  = READ(HL operator);				 \
	zuint8 t0 = A - n;					 \
	zuint8 hf = (A ^ n ^ t0) & HF;				 \
	zuint8 t1 = t0 - (hf >> 4);				 \
								 \
	FLAGS = (zuint8)(					 \
		(t0 & SF)	| /* SF = sign		      */ \
		ZF_ZERO(t0)	| /* ZF = zero		      */ \
		hf		| /* HF = half-borrow	      */ \
		((t1 & 2) << 4) | /* YF = (A - [HLi] - HFo).1 */ \
		(t1 & XF)	| /* XF = (A - [HLi] - HFo).3 */ \
		(!!(--BC) << 2) | /* PF = !!BCo		      */ \
		NF		| /* NF = 1		      */ \
		F_C);		  /* CF unchanged	      */ \
								 \
	MEMPTR operator;					 \
	PC += 2;						 \
	return 16


#define CPXR(operator)						 \
	zuint8 n  = READ(HL operator);				 \
	zuint8 t0 = A - n;					 \
	zuint8 hf = (A ^ n ^ t0) & HF;				 \
	zuint8 t1 = t0 - (hf >> 4);				 \
								 \
	zuint8 f = (zuint8)(					 \
		(t0 & SF)	| /* SF = sign	      */	 \
		ZF_ZERO(t0)	| /* ZF = zero	      */	 \
		hf		| /* HF = half-borrow */	 \
		(!!(--BC) << 2) | /* PF = !!BCo	      */	 \
		NF		| /* NF = 1	      */	 \
		F_C);		  /* CF unchanged     */	 \
								 \
	if (t0 && BC)						 \
		{						 \
		/* YF = PCi.13; XF = PCi.11 */			 \
		FLAGS = f | ((PC >> 8) & YXF);			 \
		MEMPTR = PC + 1;				 \
		return 21;					 \
		}						 \
								 \
	FLAGS = (zuint8)(					 \
		f		|				 \
		((t1 & 2) << 4) | /* YF = (A - [HLi] - HFo).1 */ \
		(t1 & XF));	  /* XF = (A - [HLi] - HFo).3 */ \
								 \
	MEMPTR operator;					 \
	PC += 2;						 \
	return 16


#define ADD_RR_NN(RR, NN)							      \
	zuint16 nn = NN;							      \
	zuint16 t  = RR + nn;							      \
										      \
	FLAGS = F_SZP				     | /* SF, ZF, PF unchanged	   */ \
		((t >> 8) & YXF)		     | /* YF = high-Y; XF = high-X */ \
		(((zuint16)(RR ^ nn ^ t) >> 8) & HF) | /* HF = high-half-carry	   */ \
		((zuint32)RR + nn > 65535);	       /* CF = carry		   */ \
						       /* NF = 0		   */ \
	MEMPTR = RR + 1;							      \
	RR = t


#define ADC_SBC_HL_SS(operator, pf_test_rhs, cf_test, set_nf)		    \
	zuint8	fc = F_C;						    \
	zuint16 ss = SS1;						    \
	zuint16 t  = HL operator ss operator fc;			    \
									    \
	FLAGS = (zuint8)(						    \
		((t >> 8) & SYXF) /* SF = sign; YF = high-Y; XF = high-X */ \
		| ZF_ZERO(t)	  /* ZF = zero				 */ \
		  /* HF = high-half-carry (adc), high-half-borrow (sbc)	 */ \
		| (((zuint16)(HL ^ ss ^ t) >> 8) & HF)			    \
		| PF_OVERFLOW(16, t, HL, pf_test_rhs) /* PF = overflow   */ \
		| ((zuint32)cf_test) /* CF = carry (adc), borrow (sbc)	 */ \
		set_nf);	     /* NF = 0 (adc), 1 (sbc)		 */ \
									    \
	MEMPTR = HL + 1;						    \
	HL = t;								    \
	PC += 2;							    \
	return 15


#define RXA(a_to_cf, operator, fc_to_a)			  \
	zuint8 cf = a_to_cf;				  \
							  \
	A = (zuint8)((A operator 1) | fc_to_a);		  \
							  \
	FLAGS = F_SZP | /* SF, ZF, PF unchanged	       */ \
		A_YX  | /* YF = Y; XF = X	       */ \
		cf;	/* CF = Ai.7 (rla), Ai.0 (rra) */ \
			/* HF, NF = 0		       */ \
	PC++;						  \
	return 4


#define RXD(vhl_to_vhl, a_to_vhl, vhl_to_a)		       \
	zuint8 t = READ(HL);				       \
							       \
	MEMPTR = HL + 1;				       \
	WRITE(HL, (zuint8)((t vhl_to_vhl) | (A a_to_vhl)));    \
	A = (A & 0xF0) | (t vhl_to_a);			       \
							       \
	FLAGS = (zuint8)(      /* HF, NF = 0;		    */ \
		A_SYX	     | /* SF = sign; YF = Y; XF = X */ \
		ZF_ZERO(A)   | /* ZF = zero		    */ \
		PF_PARITY(A) | /* PF = parity		    */ \
		F_C);	       /* CF unchanged		    */ \
							       \
	PC += 2;					       \
	return 18


#define DJNZ_JR_Z_OFFSET(condition, cycles_if_true, cycles_if_false) \
	zsint8 offset;						     \
								     \
	Q_0							     \
	offset = (zsint8)FETCH(PC + 1); /* always */		     \
								     \
	if (condition)						     \
		{						     \
		MEMPTR = (PC += 2 + offset);			     \
		return cycles_if_true;				     \
		}						     \
								     \
	PC += 2;						     \
	return cycles_if_false


#define RETX(mnemonic)						   \
	NOTIFY(mnemonic);					   \
	Q_0							   \
	DATA[2] = IFF1;						   \
	if ((IFF1 = IFF2) && INT_LINE) REQUEST |= Z80_REQUEST_INT; \
	RET;							   \
	return 14


#define IN_VC						       \
	zuint8 t;					       \
							       \
	MEMPTR = BC + 1;				       \
	t = IN(BC);					       \
							       \
	FLAGS = (zuint8)(      /* HF, NF = 0		    */ \
		(t & SYXF)   | /* SF = sign; YF = Y; XF = X */ \
		ZF_ZERO(t)   | /* ZF = zero		    */ \
		PF_PARITY(t) | /* PF = parity		    */ \
		F_C)	       /* CF unchanged		    */


#define INX_OUTX(io)							       \
	FLAGS = (zuint8)(						       \
		(B & SYXF)	       | /* SF = Bo.7; YF = Bo.5; XF = Bo.3 */ \
		ZF_ZERO(B)	       | /* ZF = !Bo			    */ \
		PF_PARITY((t & 7) ^ B) | /* PF = ((T & 7) ^ Bo).parity	    */ \
		((t > 255) ? HCF : 0)  | /* HF, CF = T > 255		    */ \
		((io >> 6) & NF));	 /* NF = IO.7			    */ \
									       \
	PC += 2;							       \
	return 16


#define INX(hl_operator, memptr_operator)		       \
	zuint8 in = IN(BC);				       \
	zuint  t  = (zuint)in + (zuint8)(C memptr_operator 1); \
							       \
	WRITE(HL hl_operator, in);			       \
	MEMPTR = BC memptr_operator 1;			       \
	B--;						       \
	INX_OUTX(in)					       \


#define OUTX(hl_operator, memptr_operator) \
	zuint8 out = READ(HL hl_operator); \
	zuint  t   = (zuint)out + L;	   \
					   \
	B--;				   \
	MEMPTR = BC memptr_operator 1;	   \
	OUT(BC, out);			   \
	INX_OUTX(out)


#define INXR_OTXR(io)						      \
	if (B)	{						      \
		FLAGS = (zuint8)(     /* ZF = 0			  */  \
			(B & SF)    | /* SF = Bo.7		  */  \
			(PCH & YXF) | /* YF = PCi.13; XF = PCi.11 */  \
			nf	    | /* NF = IO.7		  */  \
			(hcf	?				      \
				CF |				      \
				(nf	?			      \
					(!(B & 0xF) << 4) |	      \
					PF_PARITY(p ^ ((B - 1) & 7))  \
					:			      \
					(((B & 0xF) == 0xF) << 4) |   \
					PF_PARITY(p ^ ((B + 1) & 7))) \
				: PF_PARITY(p ^ (B & 7))));	      \
								      \
		return 21;					      \
		}						      \
								      \
	FLAGS = ZF	     | /* ZF = 1; SF, YF, XF = 0     */	      \
		hcf	     | /* HF, CF = T > 255	     */	      \
		PF_PARITY(p) | /* PF = ((T & 7) ^ Bo).parity */	      \
		nf;	       /* NF = IO.7		     */	      \
								      \
	PC += 2;						      \
	return 16


#define INXR(hl_operator, memptr_operator)			\
	zuint8 in  = IN(BC);					\
	zuint8 nf  = (in >> 6) & NF;				\
	zuint  t   = (zuint)in + (zuint8)(C memptr_operator 1); \
	zuint8 hcf = (t > 255) ? HCF : 0;			\
	zuint8 p;						\
								\
	WRITE(HL hl_operator, in);				\
	MEMPTR = BC memptr_operator 1;				\
	p = (t & 7) ^ --B;					\
	INXR_OTXR(in)


#define OTXR(hl_operator, memptr_operator) \
	zuint8 out = READ(HL hl_operator); \
	zuint8 nf  = (out >> 6) & NF;	   \
	zuint  t   = (zuint)out + L;	   \
	zuint8 hcf = (t > 255) ? HCF : 0;  \
	zuint8 p   = (t & 7) ^ --B;	   \
					   \
	MEMPTR = BC memptr_operator 1;	   \
	OUT(BC, out);			   \
	INXR_OTXR(out)


#define SET_HALT_LINE(state) \
	HALT_LINE = state;   \
	if (self->halt != Z_NULL) self->halt(CONTEXT, state)


/* MARK: - Instructions: 8-Bit Load Group */
/*----------------------------------------------------------------------------.
|			0	1	2	3	  Flags	    T-states  |
|  Assembly		76543210765432107654321076543210  SZYHXPNC     12345  |
|  -------------------	--------------------------------  --------  --------  |
|  ld J,K		01jjjkkk			  ........   4:4      |
|* ld O,P		<--XY-->01oooppp		  ........   8:44     |
|  ld J,BYTE		00jjj110<-BYTE->		  ........   7:43     |
|* ld O,BYTE		<--XY-->00ooo110<-BYTE->	  ........  11:443    |
|  ld J,(hl)		01jjj110			  ........   7:43     |
|  ld J,(XY+OFFSET)	<--XY-->01jjj110<OFFSET>	  ........  19:44353  |
|  ld (hl),K		01110kkk			  ........   7:43     |
|  ld (XY+OFFSET),K	<--XY-->01110kkk<OFFSET>	  ........  19:44353  |
|  ld (hl),BYTE		<--36--><-BYTE->		  ........  10:433    |
|  ld (XY+OFFSET),BYTE	<--XY--><--36--><OFFSET><-BYTE->  ........  19:44353  |
|  ld a,(bc)		<--0A-->			  ........   7:43     |
|  ld a,(de)		<--1A-->			  ........   7:43     |
|  ld a,(WORD)		<--3A--><-----WORD----->	  ........  13:4333   |
|  ld (bc),a		<--02-->			  ........   7:43     |
|  ld (de),a		<--12-->			  ........   7:43     |
|  ld (WORD),a		<--32--><-----WORD----->	  ........  13:4333   |
|  ld a,i		<--ED--><--57-->		  szy0x*0.   9:45     |
|  ld a,r		<--ED--><--5F-->		  szy0x*0.   9:45     |
|  ld i,a		<--ED--><--47-->		  ........   9:45     |
|  ld r,a		<--ED--><--4F-->		  ........   9:45     |
|-----------------------------------------------------------------------------|
| (*) Undocumented instruction.						      |
'============================================================================*/

INSTRUCTION(ld_J_K	   ) {Q_0 J0 = K0;					PC++;	 return  4;}
INSTRUCTION(ld_O_P	   ) {Q_0 O  = P;					PC += 2; return  4;}
INSTRUCTION(ld_J_BYTE	   ) {Q_0 J0 = FETCH((PC += 2) - 1);				 return  7;}
INSTRUCTION(ld_O_BYTE	   ) {Q_0 O  = FETCH((PC += 3) - 1);				 return  7;}
INSTRUCTION(ld_J_vhl	   ) {Q_0 J0 = READ(HL);				PC++;	 return  7;}
INSTRUCTION(ld_J_vXYpOFFSET) {Q_0 J1 = READ(FETCH_XY_EA((PC += 3) - 1));		 return 15;}
INSTRUCTION(ld_vhl_K	   ) {Q_0 WRITE(HL, K0);				PC++;	 return  7;}
INSTRUCTION(ld_vXYpOFFSET_K) {Q_0 WRITE(FETCH_XY_EA((PC += 3) - 1), K1);		 return 15;}
INSTRUCTION(ld_vhl_BYTE	   ) {Q_0 WRITE(HL,   FETCH((PC += 2) - 1));			 return 10;}
INSTRUCTION(ld_a_vbc	   ) {Q_0 MEMPTR  = BC + 1; A = READ(BC);		PC++;	 return  7;}
INSTRUCTION(ld_a_vde	   ) {Q_0 MEMPTR  = DE + 1; A = READ(DE);		PC++;	 return  7;}
INSTRUCTION(ld_a_vWORD	   ) {Q_0 MEMPTR  = FETCH_16((PC += 3) - 2); A = READ(MEMPTR++); return 13;}
INSTRUCTION(ld_vbc_a	   ) {Q_0 MEMPTRL = C + 1; WRITE(BC, MEMPTRH = A);	PC++;	 return  7;}
INSTRUCTION(ld_vde_a	   ) {Q_0 MEMPTRL = E + 1; WRITE(DE, MEMPTRH = A);	PC++;	 return  7;}
INSTRUCTION(ld_a_i	   ) {LD_A_IR(I);							   }
INSTRUCTION(ld_a_r	   ) {LD_A_IR(R_ALL);							   }
INSTRUCTION(ld_i_a	   ) {NOTIFY(ld_i_a); Q_0 I = A;			PC += 2; return  9;}
INSTRUCTION(ld_r_a	   ) {NOTIFY(ld_r_a); Q_0 R = R7 = A;			PC += 2; return  9;}


INSTRUCTION(ld_vXYpOFFSET_BYTE)
	{
	zuint16 ea;

	Q_0
 	ea = FETCH_XY_EA((PC += 4) - 2);
	WRITE(ea, FETCH(PC - 1));
	return 15;
	}


INSTRUCTION(ld_vWORD_a)
	{
	zuint16 ea;

	Q_0
	MEMPTRL = (zuint8)((ea = FETCH_16((PC += 3) - 2)) + 1);
	WRITE(ea, MEMPTRH = A);
	return 13;
	}


/* MARK: - Instructions: 16-Bit Load Group */
/*----------------------------------------------------------------------.
|		 0	 1	 2	 3	   Flags     T-states	|
|  Assembly	 76543210765432107654321076543210  SZYHXPNC	123456	|
|  ------------	 --------------------------------  --------  ---------	|
|  ld SS,WORD	 00ss0001<-----WORD----->	   ........  10:433	|
|  ld XY,WORD	 <--XY--><--21--><-----WORD----->  ........  14:4433	|
|  ld hl,(WORD)	 <--2A--><-----WORD----->	   ........  16:43333	|
|  ld SS,(WORD)	 <--ED-->01ss1011<-----WORD----->  ........  20:443333	|
|  ld XY,(WORD)	 <--XY--><--2A--><-----WORD----->  ........  20:443333	|
|  ld (WORD),hl	 <--22--><-----WORD----->	   ........  16:43333	|
|  ld (WORD),SS	 <--ED-->01ss0011<-----WORD----->  ........  20:443333	|
|  ld (WORD),XY	 <--XY--><--22--><-----WORD----->  ........  20:443333	|
|  ld sp,hl	 <--F9-->			   ........   6:6	|
|  ld sp,XY	 <--XY--><--F9-->		   ........  10:46	|
|  push TT	 11tt0101			   ........  11:533	|
|  push XY	 <--XY--><--E5-->		   ........  15:4533	|
|  pop TT	 11tt0001			   ........  10:433	|
|  pop XY	 <--XY--><--E1-->		   ........  14:4433	|
'======================================================================*/

INSTRUCTION(ld_SS_WORD ) {Q_0 SS0    = FETCH_16((PC += 3) - 2);				  return 10;}
INSTRUCTION(ld_XY_WORD ) {Q_0 XY     = FETCH_16((PC += 4) - 2);				  return 10;}
INSTRUCTION(ld_hl_vWORD) {Q_0 MEMPTR = FETCH_16((PC += 3) - 2); HL  = READ_16(MEMPTR++);  return 16;}
INSTRUCTION(ld_SS_vWORD) {Q_0 MEMPTR = FETCH_16((PC += 4) - 2); SS1 = READ_16(MEMPTR++);  return 20;}
INSTRUCTION(ld_XY_vWORD) {Q_0 MEMPTR = FETCH_16((PC += 4) - 2); XY  = READ_16(MEMPTR++);  return 16;}
INSTRUCTION(ld_vWORD_hl) {Q_0 MEMPTR = FETCH_16((PC += 3) - 2); WRITE_16F(MEMPTR++, HL ); return 16;}
INSTRUCTION(ld_vWORD_SS) {Q_0 MEMPTR = FETCH_16((PC += 4) - 2); WRITE_16F(MEMPTR++, SS1); return 20;}
INSTRUCTION(ld_vWORD_XY) {Q_0 MEMPTR = FETCH_16((PC += 4) - 2); WRITE_16F(MEMPTR++, XY ); return 16;}
INSTRUCTION(ld_sp_hl   ) {Q_0 SP = HL;						 PC++;	  return  6;}
INSTRUCTION(ld_sp_XY   ) {Q_0 SP = XY;						 PC += 2; return  6;}
INSTRUCTION(push_TT    ) {Q_0 WRITE_16B(SP -= 2, TT);				 PC++;	  return 11;}
INSTRUCTION(push_XY    ) {Q_0 WRITE_16B(SP -= 2, XY);				 PC += 2; return 11;}
INSTRUCTION(pop_TT     ) {Q_0 TT = READ_16(SP); SP += 2;			 PC++;	  return 10;}
INSTRUCTION(pop_XY     ) {Q_0 XY = READ_16(SP); SP += 2;			 PC += 2; return 10;}


/* MARK: - Instructions: Exchange, Block Transfer and Search Groups */
/*-------------------------------------------------------------.
|	       0       1	 Flags	   T-states	       |
|  Assembly    7654321076543210	 SZYHXPNC  !0 123456  =0 1234  |
|  ----------  ----------------	 --------  ------------------  |
|  ex de,hl    <--EB-->		 ........   4:4		       |
|  ex af,af'   <--08-->		 ........   4:4		       |
|  exx	       <--D9-->		 ........   4:4		       |
|  ex (sp),hl  <--E3-->		 ........  19:43435	       |
|  ex (sp),XY  <--XY--><--E3-->	 ........  23:443435	       |
|  ldi	       <--ED--><--A0-->	 ..*0**0.  16:4435	       |
|  ldir	       <--ED--><--B0-->	 ..*0*00.  21:44355   16:4435  |
|  ldd	       <--ED--><--A8-->	 ..*0**0.  16:4435	       |
|  lddr	       <--ED--><--B8-->	 ..*0*00.  21:44355   16:4435  |
|  cpi	       <--ED--><--A1-->	 sz*b**1.  16:4435	       |
|  cpir	       <--ED--><--B1-->	 sz*b**1.  21:44355   16:4435  |
|  cpd	       <--ED--><--A9-->	 sz*b**1.  16:4435	       |
|  cpdr	       <--ED--><--B9-->	 sz*b**1.  21:44355   16:4435  |
'=============================================================*/

INSTRUCTION(ex_de_hl ) {zuint16 t; Q_0 EX(DE, HL );				  PC++;	   return  4;}
INSTRUCTION(ex_af_af_) {zuint16 t; Q_0 EX(AF, AF_);				  PC++;	   return  4;}
INSTRUCTION(exx      ) {zuint16 t; Q_0 EX(BC, BC_); EX(DE, DE_); EX(HL, HL_);	  PC++;	   return  4;}
INSTRUCTION(ex_vsp_hl) {Q_0 MEMPTR = READ_16(SP); WRITE_16B(SP, HL); HL = MEMPTR; PC++;	   return 19;}
INSTRUCTION(ex_vsp_XY) {Q_0 MEMPTR = READ_16(SP); WRITE_16B(SP, XY); XY = MEMPTR; PC += 2; return 19;}
INSTRUCTION(ldi      ) {LDX (++);								     }
INSTRUCTION(ldir     ) {LDXR(++);								     }
INSTRUCTION(ldd      ) {LDX (--);								     }
INSTRUCTION(lddr     ) {LDXR(--);								     }
INSTRUCTION(cpi      ) {CPX (++);								     }
INSTRUCTION(cpir     ) {CPXR(++);								     }
INSTRUCTION(cpd      ) {CPX (--);								     }
INSTRUCTION(cpdr     ) {CPXR(--);								     }


/* MARK: - Instructions: 8-Bit Arithmetic and Logical Group */
/*-------------------------------------------------------------------.
|		      0	      1	      2		Flags     T-states   |
|  Assembly	      765432107654321076543210	SZYHXPNC     123456  |
|  -----------------  ------------------------	--------  ---------  |
|  U [a,]K	      10uuukkk			sz||||||   4:4	     |
|* U [a,]P	      <--XY-->10uuuppp		sz||||||   8:44	     |
|  U [a,]BYTE	      11uuu110<-BYTE->		sz||||||   7:43	     |
|  U [a,](hl)	      10uuu110			sz||||||   7:43	     |
|  U [a,](XY+OFFSET)  <--XY-->10uuu110<OFFSET>	sz||||||  19:44353   |
|  V J		      00jjj10v			szy|xv|.   4:4	     |
|* V O		      <--XY-->00ooo10v		szy|xv|.   8:44	     |
|  V (hl)	      0011010v			szy|xv|.  11:443     |
|  V (XY+OFFSET)      <--XY-->0011010v<OFFSET>	szy|xv|.  23:443543  |
|--------------------------------------------------------------------|
| (*) Undocumented instruction.					     |
| (|) The flag is explained in a previous table.		     |
'===================================================================*/

INSTRUCTION(U_a_K	  ) {U0(K0);						      PC++;    return  4;}
INSTRUCTION(U_a_P	  ) {U1(P);						      PC += 2; return  4;}
INSTRUCTION(U_a_BYTE	  ) {U0(FETCH((PC += 2) - 1));					       return  7;}
INSTRUCTION(U_a_vhl	  ) {U0(READ(HL));					      PC++;    return  7;}
INSTRUCTION(U_a_vXYpOFFSET) {U1(READ(FETCH_XY_EA((PC += 3) - 1)));			       return 15;}
INSTRUCTION(V_J		  ) {zuint8 *j = &J0; *j = V0(*j);			      PC++;    return  4;}
INSTRUCTION(V_O		  ) {zuint8 *o = &O;  *o = V1(*o);			      PC += 2; return  4;}
INSTRUCTION(V_vhl	  ) {WRITE(HL, V0(READ(HL)));				      PC++;    return 11;}
INSTRUCTION(V_vXYpOFFSET  ) {zuint16 ea = FETCH_XY_EA((PC += 3) - 1); WRITE(ea, V1(READ(ea))); return 19;}


/* MARK: - Instructions: General-Purpose Arithmetic and CPU Control Groups */
/*-------------------------------------------------.
|	     0	     1	       Flags	 T-states  |
|  Assembly  7654321076543210  SZYHXPNC	   12	   |
|  --------  ----------------  --------	 --------  |
|  daa	     <--27-->	       szy^xp.*	 4:4	   |
|  cpl	     <--2F-->	       ..y1x.1.	 4:4	   |
|+ neg	     <--ED-->01***100  szybxv1b	 8:44	   |
|  ccf	     <--3F-->	       ..***.0~	 4:4	   |
|  scf	     <--37-->	       ..*0*.01	 4:4	   |
|  nop	     <--00-->	       ........	 4:4	   |
|  halt	     <--76-->	       ........	 4:4	   |
|  di	     <--F3-->	       ........	 4:4	   |
|  ei	     <--FB-->	       ........	 4:4	   |
|+ im 0	     <--ED-->01*0*110  ........	 8:44	   |
|+ im 1	     <--ED-->01*10110  ........	 8:44	   |
|+ im 2	     <--ED-->01*11110  ........	 8:44	   |
|--------------------------------------------------|
| (+) The instruction has undocumented opcodes.	   |
'=================================================*/

INSTRUCTION(nop ) {Q_0	       PC++;	return 4;}
INSTRUCTION(im_0) {Q_0 IM = 0; PC += 2; return 8;}
INSTRUCTION(im_1) {Q_0 IM = 1; PC += 2; return 8;}
INSTRUCTION(im_2) {Q_0 IM = 2; PC += 2; return 8;}


INSTRUCTION(daa)
	{
	zuint8 cf = A > 0x99, t = ((F & HF) || (A & 0xF) > 9) ? 6 : 0;

	if (F_C || cf) t |= 0x60;
	t = (F & NF) ? A - t : A + t;

	FLAGS = (zuint8)(
		(F & (NF | CF)) | /* NF unchanged; CF dominant */
		(t & SYXF)	| /* SF = sign; YF = Y; XF = X */
		ZF_ZERO(t)	| /* ZF = zero		       */
		((A ^ t) & HF)	| /* HF = Ai.4 != Ao.4	       */
		PF_PARITY(t)	| /* PF = parity	       */
		cf);		  /* CF |= 1 (if BCD carry)    */

	A = t;
	PC++;
	return 4;
	}


INSTRUCTION(cpl)
	{
	FLAGS = F_SZPC		 | /* SF, ZF, PF, CF unchanged */
		((A = ~A) & YXF) | /* YF = Y; XF = X	       */
		HF | NF;	   /* HF, NF = 1	       */

	PC++;
	return 4;
	}


INSTRUCTION(neg)
	{
	zuint8 t = (zuint8)-A;

	FLAGS = (zuint8)(
		(t & SYXF)	  | /* SF = sign; YF = Y; XF = X */
		ZF_ZERO(t)	  | /* ZF = zero		 */
		((A ^ t) & HF)	  | /* HF = half-borrow		 */
		((t == 128) << 2) | /* PF = overflow		 */
		NF		  | /* NF = 1			 */
		!!A);		    /* CF = borrow (not 0)	 */

	A = t;
	PC += 2;
	return 8;
	}


/*---------------------------------------------------------------------------.
| `ccf` and `scf` are the only instructions in which Q affects the flags.    |
| Patrik Rak cracked the behavior of YF and XF in 2012, confirming that they |
| are taken, respectively, from bits 5 and 3 of the result of "(Q ^ F) | A"  |
| [1,2]. This applies to all Zilog Z80 models, both NMOS and CMOS. In 2018,  |
| David Banks (AKA hoglet) found that ST CMOS models do not set XF according |
| to this formula, but take this flag from bit 3 of A, while NEC NMOS models |
| take both flags from A [3].						     |
|									     |
| References:								     |
| 1. https://worldofspectrum.org/forums/discussion/20345		     |
| 2. https://worldofspectrum.org/forums/discussion/41704		     |
| 3. Banks, David (2018-08-21). "Undocumented Z80 Flags" revision 1.0.	     |
|     * https://github.com/hoglet67/Z80Decoder/wiki/Undocumented-Flags	     |
|     * https://stardot.org.uk/forums/download/file.php?id=39831	     |
'===========================================================================*/

INSTRUCTION(ccf)
	{
	FLAGS = (zuint8)(
		(F_SZPC ^ CF) | /* SF, ZF, PF unchanged; CF = ~CFi	    */
		/* Zilog:    YF = A.5 | (YFi ^ YQi); XF = A.3 | (XFi ^ XQi) */
		/* ST CMOS:  YF = A.5 | (YFi ^ YQi); XF = A.3		    */
		/* NEC NMOS: YF = A.5; XF = A.3				    */
#		ifdef Z80_WITH_Q
			((((F ^ Q) & OPTIONS) | A) & YXF) |
#		else
			(A & YXF) |
#		endif
		(F_C << 4)); /* HF = CFi */
			     /* NF = 0	 */
	PC++;
	return 4;
	}


INSTRUCTION(scf)
	{
	FLAGS = F_SZP | /* SF, ZF, PF unchanged				    */
		/* Zilog:    YF = A.5 | (YFi ^ YQi); XF = A.3 | (XFi ^ XQi) */
		/* ST CMOS:  YF = A.5 | (YFi ^ YQi); XF = A.3		    */
		/* NEC NMOS: YF = A.5; XF = A.3				    */
#		ifdef Z80_WITH_Q
			((((F ^ Q) & OPTIONS) | A) & YXF) |
#		else
			(A & YXF) |
#		endif
		CF; /* CF = 1	  */
		    /* HF, NF = 0 */
	PC++;
	return 4;
	}


/*-----------------------------------------------------------------------------.
| The `halt` instruction enables the HALT state after PC is incremented during |
| the opcode fetch. The CPU neither decrements nor avoids incrementing PC "so  |
| that the instruction is re-executed" as Sean Young writes in section 5.4 of  |
| "The Undocumented Z80 Documented". During the HALT state, the CPU repeatedly |
| executes an internal NOP operation. Each NOP consists of 1 M1 cycle of 4     |
| T-states that fetches and disregards the next opcode after `halt` without    |
| incrementing PC. This opcode is read again and again until an exit condition |
| occurs (i.e., INT, NMI or RESET).					       |
|									       |
| This was first documented by Tony Brewer in 2014, and was later re-confirmed |
| by the HALT2INT test written by Mark Woodmass (AKA Woody) in 2021.	       |
|									       |
| References:								       |
| * Brewer, Tony (2014-12). "Z80 Special Reset".			       |
|     * http://primrosebank.net/computers/z80/z80_special_reset.htm	       |
| * https://stardot.org.uk/forums/viewtopic.php?p=357136#p357136	       |
'=============================================================================*/

#ifdef Z80_WITH_SPECIAL_RESET
	static Instruction const instruction_table[256];
#endif

INSTRUCTION(halt)
	{
	if (!HALT_LINE)
		{
		if (!RESUME)
			{
			Q_0
			PC++;
			if (REQUEST) return 4;
			RESUME = Z80_RESUME_HALT;
			if ((self->cycles += 4) >= self->cycle_limit) return 0;
			}

		SET_HALT_LINE(TRUE);
		}

	if (self->nop == Z_NULL || (OPTIONS & Z80_OPTION_HALT_SKIP))
		{
		zusize nop_cycles = self->cycle_limit - self->cycles;

		nop_cycles += (4 - (nop_cycles & 3)) & 3;
		R += (zuint8)(nop_cycles >> 2);
		self->cycles += nop_cycles;
		}

#	ifdef Z80_WITH_SPECIAL_RESET
		else	{
			zuint8 opcode;

			do	{
				R++; /* M1 */
				opcode = self->nop(CONTEXT, PC);
				self->cycles += 4;

				if (REQUEST)
					{
					RESUME = FALSE;

					if (REQUEST & Z80_REQUEST_SPECIAL_RESET)
						{
						HALT_LINE = FALSE;

						if (self->halt != Z_NULL)
							self->halt(CONTEXT, Z80_HALT_EARLY_EXIT);

						if ((DATA[0] = opcode) != 0x76)
							{
							self->cycles -= 4;
							PC--;
							return instruction_table[opcode](self);
							}
						}

					return 0;
					}
				}
			while (self->cycles < self->cycle_limit);

			DATA[2] = opcode;
			}

#	else
		else do	{
			R++; /* M1 */
			(void)self->nop(CONTEXT, PC);
			self->cycles += 4;

			if (REQUEST)
				{
				RESUME = FALSE;
				return 0;
				}
			}
		while (self->cycles < self->cycle_limit);
#	endif

	return 0;
	}


INSTRUCTION(di)
	{
	Q_0
	IFF1 = IFF2 = 0;
	REQUEST &= ~(zuint8)Z80_REQUEST_INT;
	PC++;
	return 4;
	}


INSTRUCTION(ei)
	{
	Q_0
	IFF1 = IFF2 = 1;
	if (INT_LINE) REQUEST |= Z80_REQUEST_INT;
	PC++;
	return 4;
	}


/* MARK: - Instructions: 16-Bit Arithmetic Group */
/*--------------------------------------------------.
|	      0	      1		Flags	  T-states  |
|  Assembly   7654321076543210	SZYHXPNC     1234   |
|  ---------  ----------------	--------  --------  |
|  add hl,SS  00ss1001		..YCX.0c  11:443    |
|  adc hl,SS  <--ED-->01ss1010	szYCXv0c  15:4443   |
|  sbc hl,SS  <--ED-->01ss0010	szYBXv1b  15:4443   |
|  add XY,WW  <--XY-->00ww1001	..YCX.0c  15:4443   |
|  inc SS     00ss0011		........   6:6	    |
|  inc XY     <--XY--><--23-->	........  10:46	    |
|  dec SS     00ss1011		........   6:6	    |
|  dec XY     <--XY--><--2B-->	........  10:46	    |
'==================================================*/

INSTRUCTION(add_hl_SS) {ADD_RR_NN(HL, SS0);		  PC++;    return 11;}
INSTRUCTION(adc_hl_SS) {ADC_SBC_HL_SS(+, ~ss, ss + fc + HL > 65535, Z_EMPTY);}
INSTRUCTION(sbc_hl_SS) {ADC_SBC_HL_SS(-,  ss, ss + fc	   > HL,    | NF   );}
INSTRUCTION(add_XY_WW) {ADD_RR_NN(XY, WW);		  PC += 2; return 11;}
INSTRUCTION(inc_SS   ) {Q_0 (SS0)++;			  PC++;    return  6;}
INSTRUCTION(inc_XY   ) {Q_0 XY++;			  PC += 2; return  6;}
INSTRUCTION(dec_SS   ) {Q_0 (SS0)--;			  PC++;    return  6;}
INSTRUCTION(dec_XY   ) {Q_0 XY--;			  PC += 2; return  6;}


/* MARK: - Instructions: Rotate and Shift Group */
/*-------------------------------------------------------------------------.
|		    0	    1	    2	    3	      Flags	T-states   |
|  Assembly	    76543210765432107654321076543210  SZYHXPNC	   123456  |
|  ---------------  --------------------------------  --------	---------  |
|  rlca		    <--07-->			      ..y0x.0=	 4:4	   |
|  rla		    <--17-->			      ..y0x.0=	 4:4	   |
|  rrca		    <--0F-->			      ..y0x.0=	 4:4	   |
|  rra		    <--1F-->			      ..y0x.0=	 4:4	   |
|+ G K		    <--CB-->00gggkkk		      szy0xp0=	 8:44	   |
|+ G (hl)	    <--CB-->00ggg110		      szy0xp0=	15:4443	   |
|+ G (XY+OFFSET)    <--XY--><--CB--><OFFSET>00ggg110  szy0xp0=	23:443543  |
|* G (XY+OFFSET),K  <--XY--><--CB--><OFFSET>00gggkkk  szy0xp0=	23:443543  |
|  rld		    <--ED--><--6F-->		      szy0xp0.	18:44343   |
|  rrd		    <--ED--><--67-->		      szy0xp0.	18:44343   |
|--------------------------------------------------------------------------|
| (+) Some forms of the instruction are undocumented.			   |
| (*) Undocumented instruction.						   |
'=========================================================================*/

INSTRUCTION(rlca	  ) {A = ROL(A); FLAGS = F_SZP | (A & YXCF);	  PC++; return	4;}
INSTRUCTION(rla		  ) {RXA(A >> 7, <<, F_C);					  }
INSTRUCTION(rrca	  ) {A = ROR(A); FLAGS = F_SZP | A_YX | (A >> 7); PC++; return	4;}
INSTRUCTION(rra		  ) {RXA(A & 1, >>, (F << 7));					  }
INSTRUCTION(G_K		  ) {zuint8 *k = &K1; *k = G1(*k);			return	8;}
INSTRUCTION(G_vhl	  ) {WRITE(HL, G1(READ(HL)));				return 15;}
INSTRUCTION(G_vXYpOFFSET  ) {zuint16 ea = MEMPTR; WRITE(ea,	 G3(READ(ea))); return 19;}
INSTRUCTION(G_vXYpOFFSET_K) {zuint16 ea = MEMPTR; WRITE(ea, K3 = G3(READ(ea))); return 19;}
INSTRUCTION(rld		  ) {RXD(<< 4, & 0xF, >> 4);					  }
INSTRUCTION(rrd		  ) {RXD(>> 4, << 4, & 0xF);					  }


/* MARK: - Instructions: Bit Set, Reset and Test Group */
/*---------------------------------------------------------------------------.
|		      0	      1	      2	      3		Flags	  T-states   |
|  Assembly	      76543210765432107654321076543210	SZYHXPNC     123456  |
|  -----------------  --------------------------------	--------  ---------  |
|  bit N,K	      <--CB-->01nnnkkk			sz*1*z0.   8:44	     |
|  bit N,(hl)	      <--CB-->01nnn110			sz*1*z0.  12:444    [1]
|+ bit N,(XY+OFFSET)  <--XY--><--CB--><OFFSET>01nnn***	sz*1*z0.  20:44354   |
|  M N,K	      <--CB-->1mnnnkkk			........   8:44	     |
|  M N,(hl)	      <--CB-->1mnnn110			........  15:4443    |
|  M N,(XY+OFFSET)    <--XY--><--CB--><OFFSET>1mnnn110	........  23:443543  |
|* M N,(XY+OFFSET),K  <--XY--><--CB--><OFFSET>1mnnnkkk	........  23:443543  |
|----------------------------------------------------------------------------|
| (+) Some forms of the instruction are undocumented.			     |
| (*) Undocumented instruction.						     |
|----------------------------------------------------------------------------|
| 1. All versions of Zilog's "Z80 CPU User Manual" contain a typo in the     |
|    M-cycles of this instruction: an additional M-cycle of 4 T-states.      |
'===========================================================================*/

INSTRUCTION(M_N_K	    ) {zuint8 *k = &K1; *k = M1(*k);			  return  8;}
INSTRUCTION(M_N_vhl	    ) {WRITE(HL, M1(READ(HL)));				  return 15;}
INSTRUCTION(M_N_vXYpOFFSET  ) {zuint16 ea = MEMPTR; WRITE(ea,	   M3(READ(ea))); return 19;}
INSTRUCTION(M_N_vXYpOFFSET_K) {zuint16 ea = MEMPTR; WRITE(ea, K3 = M3(READ(ea))); return 19;}


INSTRUCTION(bit_N_K)
	{
	zuint8 k = K1;
	zuint8 t = k & (1U << N(1));

	/*----------------------------------------------------------.
	| In section 4.1 of "The Undocumented Z80 Documented" (all  |
	| versions), Sean Young says that YF and XF are taken from  |
	| the value resulting from the bit test operation, but this |
	| seems not to be true. They are copies of bits 5 and 3 of  |
	| the register containing the value to be tested (K).	    |
	'==========================================================*/
	FLAGS = (t ? t & SF : ZPF) | /* SF = sign; ZF, PF = zero   */
		(k & YXF)	   | /* YF = K.5; XF = K.3	   */
		HF		   | /* HF = 1			   */
		F_C;		     /* CF unchanged		   */
				     /* NF = 0			   */
	return 8;
	}


INSTRUCTION(bit_N_vhl)
	{
	zuint8 t = READ(HL) & (1U << N(1));

	/*----------------------------------------------------------------.
	| This is the only instruction in which MEMPTR affects the flags. |
	| YF and XF are taken, respectively, from bits 13 and 11 of this  |
	| internal register whose behavior was cracked in 2006 by boo_boo |
	| and Vladimir Kladov. Official schematics refer to this register |
	| as WZ, but this emulator uses the name "MEMPTR" to honour those |
	| who cracked it.						  |
	|								  |
	| References:							  |
	| * https://zxpress.ru/zxnet/zxnet.pc/5909			  |
	| * boo_boo; Kladov, Vladimir (2006-03-29). "MEMPTR, Esoteric	  |
	|   Register of the Zilog Z80 CPU".				  |
	|     * http://zx.pk.ru/showpost.php?p=43688			  |
	|     * http://zx.pk.ru/attachment.php?attachmentid=2984	  |
	|     * http://zx.pk.ru/showpost.php?p=43800			  |
	|     * http://zx.pk.ru/attachment.php?attachmentid=2989	  |
	'================================================================*/
	FLAGS = (t ? t & SF : ZPF) | /* SF = sign; ZF, PF = zero	 */
		(MEMPTRH & YXF)	   | /* YF = MEMPTRH.5; XF = MEMPTRH.3	 */
		HF		   | /* HF = 1				 */
		F_C;		     /* CF unchanged			 */
				     /* NF = 0				 */
	return 12;
	}


INSTRUCTION(bit_N_vXYpOFFSET)
	{
	zuint8 t = READ(MEMPTR) & (1U << N(3));

	FLAGS = (t ? t & SF : ZPF) | /* SF sign; ZF, PF = zero */
		(MEMPTRH & YXF)	   | /* YF = EA.13; XF = EA.11 */
		HF		   | /* HF = 1		       */
		F_C;		     /* CF unchanged	       */
				     /* NF = 0		       */
	return 16;
	}


/* MARK: - Instructions: Jump Group */
/*----------------------------------------------------------------.
|		0	1	2	  Flags	    T-states	  |
|  Assembly	765432107654321076543210  SZYHXPNC   Y 123  N 12  |
|  -----------	------------------------  --------  ------------  |
|  jp WORD	<--C3--><-----WORD----->  ........  10:433	  |
|  jp Z,WORD	11zzz010<-----WORD----->  ........  10:433	  |
|  jr OFFSET	<--18--><OFFSET>	  ........  12:435	  |
|  jr Z,OFFSET	001zz000<OFFSET>	  ........  12:435  7:43  |
|  jp (hl)	<--E9-->		  ........   4:4	  |
|  jp (XY)	<--XY--><--E9-->	  ........   8:44	  |
|  djnz OFFSET	<--10--><OFFSET>	  ........  13:535  8:53  |
'================================================================*/

INSTRUCTION(jp_WORD    ) {Q_0 MEMPTR = PC = FETCH_16(PC + 1);			      return 10;}
INSTRUCTION(jp_Z_WORD  ) {Q_0 MEMPTR = FETCH_16(PC + 1); PC = Z(7) ? MEMPTR : PC + 3; return 10;}
INSTRUCTION(jr_OFFSET  ) {Q_0 MEMPTR = (PC += 2 + (zsint8)FETCH(PC + 1));	      return 12;}
INSTRUCTION(jr_Z_OFFSET) {DJNZ_JR_Z_OFFSET(Z(3), 12, 7);					}
INSTRUCTION(jp_hl      ) {Q_0 PC = HL;						      return  4;}
INSTRUCTION(jp_XY      ) {Q_0 PC = XY;						      return  4;}
INSTRUCTION(djnz_OFFSET) {DJNZ_JR_Z_OFFSET(--B, 13, 8);						}


/* MARK: - Instructions: Call and Return Group */
/*--------------------------------------------------------------------.
|		0	1	2	  Flags	    T-states	      |
|  Assembly	765432107654321076543210  SZYHXPNC   Y 123     N 123  |
|  -----------	------------------------  --------  ----------------  |
|  call WORD	<--CD--><-----WORD----->  ........  17:43433	      |
|  call Z,WORD	11zzz100<-----WORD----->  ........  17:43433  10:433  |
|  ret		<--C9-->		  ........  10:433	      |
|  ret Z	11zzz000		  ........  11:533     5:5    |
|+ reti/retn	<--ED-->01***101	  ........  14:4433	      |
|  rst N	11nnn111		  ........  11:533	      |
|---------------------------------------------------------------------|
| (+) The instruction has undocumented opcodes. The `reti` mnemonic   |
|     is used to represent the ED4Dh opcode, which is recognized by   |
|     the Z80 CTC chip. All other opcodes are represented as `retn`.  |
'====================================================================*/

INSTRUCTION(call_WORD) {Q_0 MEMPTR = FETCH_16(PC + 1); PUSH(PC + 3); PC = MEMPTR; return 17;}
INSTRUCTION(ret      ) {Q_0 RET;						  return 10;}
INSTRUCTION(ret_Z    ) {Q_0 if (Z(7)) {RET; return 11;}		     PC++;	  return  5;}
INSTRUCTION(reti     ) {RETX(reti);							    }
INSTRUCTION(retn     ) {RETX(retn);							    }
INSTRUCTION(rst_N    ) {Q_0 PUSH(PC + 1); MEMPTR = PC = DATA[0] & 56;		  return 11;}


INSTRUCTION(call_Z_WORD)
	{
	Q_0
	MEMPTR = FETCH_16(PC + 1); /* always */

	if (Z(7))
		{
		PUSH(PC + 3);
		PC = MEMPTR;
		return 17;
		}

	PC += 3;
	return 10;
	}


/* MARK: - Instructions: Input and Output Group */
/*--------------------------------------------------------------.
|		 0	 1	   Flags     T-states		|
|  Assembly	 7654321076543210  SZYHXPNC  !0 12345  =0 1234  |
|  ------------	 ----------------  --------  -----------------	|
|  in a,(BYTE)	 <--DB--><-BYTE->  ........  11:434		|
|  in J,(c)	 <--ED-->01jjj000  szy0xp0.  12:444		|
|* in (c)	 <--ED--><--70-->  szy0xp0.  12:444		|
|  ini		 <--ED--><--A2-->  ********  16:4543	       [1]
|  inir		 <--ED--><--B2-->  ********  21:45435  16:4543 [1]
|  ind		 <--ED--><--AA-->  ********  16:4543	       [1]
|  indr		 <--ED--><--BA-->  ********  21:45435  16:4543 [1]
|  out (BYTE),a	 <--D3--><-BYTE->  ........  11:434		|
|  out (c),J	 <--ED-->01jjj001  ........  12:444		|
|* out (c),0	 <--ED--><--71-->  ........  12:444		|
|  outi		 <--ED--><--A3-->  ********  16:4534		|
|  otir		 <--ED--><--B3-->  ********  21:45345  16:4534	|
|  outd		 <--ED--><--AB-->  ********  16:4534		|
|  otdr		 <--ED--><--BB-->  ********  21:45345  16:4534	|
|---------------------------------------------------------------|
| (*) Undocumented instruction.					|
|---------------------------------------------------------------|
| 1. All versions of Zilog's "Z80 CPU User Manual" have a typo  |
|    in the M-cycles of these instructions: the T-states of the |
|    3rd and 4th M-cycles are swapped.		                |
'==============================================================*/

INSTRUCTION(in_J_vc ) {IN_VC; J1 = t;			 PC += 2; return 12;}
INSTRUCTION(in_vc   ) {IN_VC;				 PC += 2; return 12;}
INSTRUCTION(ini     ) {INX (++, +);					    }
INSTRUCTION(inir    ) {INXR(++, +);					    }
INSTRUCTION(ind     ) {INX (--, -);					    }
INSTRUCTION(indr    ) {INXR(--, -);					    }
INSTRUCTION(out_vc_J) {Q_0 MEMPTR = BC + 1; OUT(BC, J1); PC += 2; return 12;}
INSTRUCTION(outi    ) {OUTX(++, +);					    }
INSTRUCTION(otir    ) {OTXR(++, +);					    }
INSTRUCTION(outd    ) {OUTX(--, -);					    }
INSTRUCTION(otdr    ) {OTXR(--, -);					    }


INSTRUCTION(in_a_vBYTE)
	{
	zuint16 t;

	Q_0

	/*--------------------------------------------------------------------.
	| In "MEMPTR, Esoteric Register of the Zilog Z80 CPU", boo_boo says   |
	| that MEMPTR is set to `((A << 8) | BYTE) + 1`, which causes a carry |
	| from the LSbyte of the resulting port number if BYTE is 255. This   |
	| differs from all other instructions where MEMPTRH is set to A, but  |
	| it has been confirmed to be correct by the IN-MEMPTR test.	      |
	'====================================================================*/
	MEMPTR = (t = (zuint16)(((zuint16)A << 8) | FETCH((PC += 2) - 1))) + 1;

	A = IN(t);
	return 11;
	}


INSTRUCTION(out_vBYTE_a)
	{
	zuint8 t;

	Q_0
	MEMPTRL = (t = FETCH((PC += 2) - 1)) + 1;
	MEMPTRH = A;
	OUT((zuint16)(((zuint16)A << 8) | t), A);
	return 11;
	}


/*----------------------------------------------------------------------------.
| The `out (c),0` instruction behaves as `out (c),255` on the Zilog Z80 CMOS. |
| This was first discovered by Simon Cooke, who reported it on Usenet in 1996 |
| [1]. Later, in 2004, Colin Piggot rediscovered it with his SAM Coupé when   |
| running a demo for SCPDU 6, coincidentally written by Simon Cooke [2]. In   |
| 2008, this was once again rediscovered by the MSX community [3].	      |
|									      |
| References:								      |
| 1. https://groups.google.com/g/comp.os.cpm/c/HfSTFpaIkuU/m/KotvMWu3bZoJ     |
| 2. https://sinclair.wiki.zxnet.co.uk/wiki/Z80				      |
| 3. https://msx.org/forum/development/msx-development/bug-z80-emulation-or-  |
|    tr-hw								      |
'============================================================================*/

INSTRUCTION(out_vc_0)
	{
	Q_0
	PC += 2;
	MEMPTR = BC + 1;
	OUT(BC, (OPTIONS & Z80_OPTION_OUT_VC_255) ? 255 : 0);
	return 12;
	}


/* MARK: - Instructions: Optimizations */

INSTRUCTION(nop_nop) {Q_0; PC += 2; return 4;}


/* MARK: - Instruction Function Tables */

INSTRUCTION(cb_prefix	);
INSTRUCTION(ed_prefix	);
INSTRUCTION(dd_prefix	);
INSTRUCTION(fd_prefix	);
INSTRUCTION(xy_cb_prefix);
INSTRUCTION(xy_xy	);
INSTRUCTION(ed_illegal	);
INSTRUCTION(xy_illegal	);
INSTRUCTION(hook	);

#ifdef Z80_WITH_UNOFFICIAL_RETI
#	define reti_retn reti
#else
#	define reti_retn retn
#endif

static Instruction const instruction_table[256] = {
/*	0	     1		 2	      3		   4		5	  6	       7	 8	      9		 A	      B		  C	       D	  E	     F */
/* 0 */ nop,	     ld_SS_WORD, ld_vbc_a,    inc_SS,	   V_J,		V_J,	  ld_J_BYTE,   rlca,	 ex_af_af_,   add_hl_SS, ld_a_vbc,    dec_SS,	  V_J,	       V_J,	  ld_J_BYTE, rrca,
/* 1 */ djnz_OFFSET, ld_SS_WORD, ld_vde_a,    inc_SS,	   V_J,		V_J,	  ld_J_BYTE,   rla,	 jr_OFFSET,   add_hl_SS, ld_a_vde,    dec_SS,	  V_J,	       V_J,	  ld_J_BYTE, rra,
/* 2 */ jr_Z_OFFSET, ld_SS_WORD, ld_vWORD_hl, inc_SS,	   V_J,		V_J,	  ld_J_BYTE,   daa,	 jr_Z_OFFSET, add_hl_SS, ld_hl_vWORD, dec_SS,	  V_J,	       V_J,	  ld_J_BYTE, cpl,
/* 3 */ jr_Z_OFFSET, ld_SS_WORD, ld_vWORD_a,  inc_SS,	   V_vhl,	V_vhl,	  ld_vhl_BYTE, scf,	 jr_Z_OFFSET, add_hl_SS, ld_a_vWORD,  dec_SS,	  V_J,	       V_J,	  ld_J_BYTE, ccf,
/* 4 */ nop,	     ld_J_K,	 ld_J_K,      ld_J_K,	   ld_J_K,	ld_J_K,	  ld_J_vhl,    ld_J_K,	 ld_J_K,      nop,	 ld_J_K,      ld_J_K,	  ld_J_K,      ld_J_K,	  ld_J_vhl,  ld_J_K,
/* 5 */ ld_J_K,	     ld_J_K,	 nop,	      ld_J_K,	   ld_J_K,	ld_J_K,	  ld_J_vhl,    ld_J_K,	 ld_J_K,      ld_J_K,	 ld_J_K,      nop,	  ld_J_K,      ld_J_K,	  ld_J_vhl,  ld_J_K,
/* 6 */ ld_J_K,	     ld_J_K,	 ld_J_K,      ld_J_K,	   hook,	ld_J_K,	  ld_J_vhl,    ld_J_K,	 ld_J_K,      ld_J_K,	 ld_J_K,      ld_J_K,	  ld_J_K,      nop,	  ld_J_vhl,  ld_J_K,
/* 7 */ ld_vhl_K,    ld_vhl_K,	 ld_vhl_K,    ld_vhl_K,	   ld_vhl_K,	ld_vhl_K, halt,	       ld_vhl_K, ld_J_K,      ld_J_K,	 ld_J_K,      ld_J_K,	  ld_J_K,      ld_J_K,	  ld_J_vhl,  nop,
/* 8 */ U_a_K,	     U_a_K,	 U_a_K,	      U_a_K,	   U_a_K,	U_a_K,	  U_a_vhl,     U_a_K,	 U_a_K,	      U_a_K,	 U_a_K,	      U_a_K,	  U_a_K,       U_a_K,	  U_a_vhl,   U_a_K,
/* 9 */ U_a_K,	     U_a_K,	 U_a_K,	      U_a_K,	   U_a_K,	U_a_K,	  U_a_vhl,     U_a_K,	 U_a_K,	      U_a_K,	 U_a_K,	      U_a_K,	  U_a_K,       U_a_K,	  U_a_vhl,   U_a_K,
/* A */ U_a_K,	     U_a_K,	 U_a_K,	      U_a_K,	   U_a_K,	U_a_K,	  U_a_vhl,     U_a_K,	 U_a_K,	      U_a_K,	 U_a_K,	      U_a_K,	  U_a_K,       U_a_K,	  U_a_vhl,   U_a_K,
/* B */ U_a_K,	     U_a_K,	 U_a_K,	      U_a_K,	   U_a_K,	U_a_K,	  U_a_vhl,     U_a_K,	 U_a_K,	      U_a_K,	 U_a_K,	      U_a_K,	  U_a_K,       U_a_K,	  U_a_vhl,   U_a_K,
/* C */ ret_Z,	     pop_TT,	 jp_Z_WORD,   jp_WORD,	   call_Z_WORD, push_TT,  U_a_BYTE,    rst_N,	 ret_Z,	      ret,	 jp_Z_WORD,   cb_prefix,  call_Z_WORD, call_WORD, U_a_BYTE,  rst_N,
/* D */ ret_Z,	     pop_TT,	 jp_Z_WORD,   out_vBYTE_a, call_Z_WORD, push_TT,  U_a_BYTE,    rst_N,	 ret_Z,	      exx,	 jp_Z_WORD,   in_a_vBYTE, call_Z_WORD, dd_prefix, U_a_BYTE,  rst_N,
/* E */ ret_Z,	     pop_TT,	 jp_Z_WORD,   ex_vsp_hl,   call_Z_WORD, push_TT,  U_a_BYTE,    rst_N,	 ret_Z,	      jp_hl,	 jp_Z_WORD,   ex_de_hl,	  call_Z_WORD, ed_prefix, U_a_BYTE,  rst_N,
/* F */ ret_Z,	     pop_TT,	 jp_Z_WORD,   di,	   call_Z_WORD, push_TT,  U_a_BYTE,    rst_N,	 ret_Z,	      ld_sp_hl,	 jp_Z_WORD,   ei,	  call_Z_WORD, fd_prefix, U_a_BYTE,  rst_N
};

static Instruction const cb_instruction_table[256] = {
/*	0	 1	  2	   3	    4	     5	      6		 7	  8	   9	    A	     B	      C	       D	E	   F */
/* 0 */ G_K,	 G_K,	  G_K,	   G_K,	    G_K,     G_K,     G_vhl,	 G_K,	  G_K,	   G_K,	    G_K,     G_K,     G_K,     G_K,	G_vhl,	   G_K,
/* 1 */ G_K,	 G_K,	  G_K,	   G_K,	    G_K,     G_K,     G_vhl,	 G_K,	  G_K,	   G_K,	    G_K,     G_K,     G_K,     G_K,	G_vhl,	   G_K,
/* 2 */ G_K,	 G_K,	  G_K,	   G_K,	    G_K,     G_K,     G_vhl,	 G_K,	  G_K,	   G_K,	    G_K,     G_K,     G_K,     G_K,	G_vhl,	   G_K,
/* 3 */ G_K,	 G_K,	  G_K,	   G_K,	    G_K,     G_K,     G_vhl,	 G_K,	  G_K,	   G_K,	    G_K,     G_K,     G_K,     G_K,	G_vhl,	   G_K,
/* 4 */ bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_vhl, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_vhl, bit_N_K,
/* 5 */ bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_vhl, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_vhl, bit_N_K,
/* 6 */ bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_vhl, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_vhl, bit_N_K,
/* 7 */ bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_vhl, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_K, bit_N_vhl, bit_N_K,
/* 8 */ M_N_K,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_vhl,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,	M_N_vhl,   M_N_K,
/* 9 */ M_N_K,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_vhl,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,	M_N_vhl,   M_N_K,
/* A */ M_N_K,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_vhl,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,	M_N_vhl,   M_N_K,
/* B */ M_N_K,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_vhl,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,	M_N_vhl,   M_N_K,
/* C */ M_N_K,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_vhl,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,	M_N_vhl,   M_N_K,
/* D */ M_N_K,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_vhl,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,	M_N_vhl,   M_N_K,
/* E */ M_N_K,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_vhl,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,	M_N_vhl,   M_N_K,
/* F */ M_N_K,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_vhl,	 M_N_K,	  M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,   M_N_K,	M_N_vhl,   M_N_K
};

static Instruction const ed_instruction_table[256] = {
/*	0	    1		2	    3		 4	     5		 6	     7		 8	     9		 A	     B		  C	      D		  E	      F */
/* 0 */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* 1 */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* 2 */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* 3 */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* 4 */ in_J_vc,    out_vc_J,	sbc_hl_SS,  ld_vWORD_SS, neg,	     retn,	 im_0,	     ld_i_a,	 in_J_vc,    out_vc_J,	 adc_hl_SS,  ld_SS_vWORD, neg,	      reti,	  im_0,	      ld_r_a,
/* 5 */ in_J_vc,    out_vc_J,	sbc_hl_SS,  ld_vWORD_SS, neg,	     retn,	 im_1,	     ld_a_i,	 in_J_vc,    out_vc_J,	 adc_hl_SS,  ld_SS_vWORD, neg,	      reti_retn,  im_2,	      ld_a_r,
/* 6 */ in_J_vc,    out_vc_J,	sbc_hl_SS,  ld_vWORD_SS, neg,	     retn,	 im_0,	     rrd,	 in_J_vc,    out_vc_J,	 adc_hl_SS,  ld_SS_vWORD, neg,	      reti_retn,  im_0,	      rld,
/* 7 */ in_vc,	    out_vc_0,	sbc_hl_SS,  ld_vWORD_SS, neg,	     retn,	 im_1,	     ed_illegal, in_J_vc,    out_vc_J,	 adc_hl_SS,  ld_SS_vWORD, neg,	      reti_retn,  im_2,	      ed_illegal,
/* 8 */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* 9 */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* A */ ldi,	    cpi,	ini,	    outi,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ldd,	     cpd,	 ind,	     outd,	  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* B */ ldir,	    cpir,	inir,	    otir,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, lddr,	     cpdr,	 indr,	     otdr,	  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* C */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* D */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* E */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal,
/* F */ ed_illegal, ed_illegal, ed_illegal, ed_illegal,	 ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal, ed_illegal,  ed_illegal, ed_illegal, ed_illegal, ed_illegal
};

static Instruction const xy_instruction_table[256] = {
/*	0		 1		  2		   3		    4		     5		      6			  7		   8	       9	   A		B	      C		  D	      E		       F */
/* 0 */ nop_nop,	 xy_illegal,	  xy_illegal,	   xy_illegal,	    V_O,	     V_O,	      ld_O_BYTE,	  xy_illegal,	   xy_illegal, add_XY_WW,  xy_illegal,	xy_illegal,   V_O,	  V_O,	      ld_O_BYTE,       xy_illegal,
/* 1 */ xy_illegal,	 xy_illegal,	  xy_illegal,	   xy_illegal,	    V_O,	     V_O,	      ld_O_BYTE,	  xy_illegal,	   xy_illegal, add_XY_WW,  xy_illegal,	xy_illegal,   V_O,	  V_O,	      ld_O_BYTE,       xy_illegal,
/* 2 */ xy_illegal,	 ld_XY_WORD,	  ld_vWORD_XY,	   inc_XY,	    V_O,	     V_O,	      ld_O_BYTE,	  xy_illegal,	   xy_illegal, add_XY_WW,  ld_XY_vWORD, dec_XY,	      V_O,	  V_O,	      ld_O_BYTE,       xy_illegal,
/* 3 */ xy_illegal,	 xy_illegal,	  xy_illegal,	   xy_illegal,	    V_vXYpOFFSET,    V_vXYpOFFSET,    ld_vXYpOFFSET_BYTE, xy_illegal,	   xy_illegal, add_XY_WW,  xy_illegal,	xy_illegal,   V_O,	  V_O,	      ld_O_BYTE,       xy_illegal,
/* 4 */ nop_nop,	 ld_O_P,	  ld_O_P,	   ld_O_P,	    ld_O_P,	     ld_O_P,	      ld_J_vXYpOFFSET,	  ld_O_P,	   ld_O_P,     nop_nop,	   ld_O_P,	ld_O_P,	      ld_O_P,	  ld_O_P,     ld_J_vXYpOFFSET, ld_O_P,
/* 5 */ ld_O_P,		 ld_O_P,	  nop_nop,	   ld_O_P,	    ld_O_P,	     ld_O_P,	      ld_J_vXYpOFFSET,	  ld_O_P,	   ld_O_P,     ld_O_P,	   ld_O_P,	nop_nop,      ld_O_P,	  ld_O_P,     ld_J_vXYpOFFSET, ld_O_P,
/* 6 */ ld_O_P,		 ld_O_P,	  ld_O_P,	   ld_O_P,	    nop_nop,	     ld_O_P,	      ld_J_vXYpOFFSET,	  ld_O_P,	   ld_O_P,     ld_O_P,	   ld_O_P,	ld_O_P,	      ld_O_P,	  nop_nop,    ld_J_vXYpOFFSET, ld_O_P,
/* 7 */ ld_vXYpOFFSET_K, ld_vXYpOFFSET_K, ld_vXYpOFFSET_K, ld_vXYpOFFSET_K, ld_vXYpOFFSET_K, ld_vXYpOFFSET_K, xy_illegal,	  ld_vXYpOFFSET_K, ld_O_P,     ld_O_P,	   ld_O_P,	ld_O_P,	      ld_O_P,	  ld_O_P,     ld_J_vXYpOFFSET, nop_nop,
/* 8 */ U_a_P,		 U_a_P,		  U_a_P,	   U_a_P,	    U_a_P,	     U_a_P,	      U_a_vXYpOFFSET,	  U_a_P,	   U_a_P,      U_a_P,	   U_a_P,	U_a_P,	      U_a_P,	  U_a_P,      U_a_vXYpOFFSET,  U_a_P,
/* 9 */ U_a_P,		 U_a_P,		  U_a_P,	   U_a_P,	    U_a_P,	     U_a_P,	      U_a_vXYpOFFSET,	  U_a_P,	   U_a_P,      U_a_P,	   U_a_P,	U_a_P,	      U_a_P,	  U_a_P,      U_a_vXYpOFFSET,  U_a_P,
/* A */ U_a_P,		 U_a_P,		  U_a_P,	   U_a_P,	    U_a_P,	     U_a_P,	      U_a_vXYpOFFSET,	  U_a_P,	   U_a_P,      U_a_P,	   U_a_P,	U_a_P,	      U_a_P,	  U_a_P,      U_a_vXYpOFFSET,  U_a_P,
/* B */ U_a_P,		 U_a_P,		  U_a_P,	   U_a_P,	    U_a_P,	     U_a_P,	      U_a_vXYpOFFSET,	  U_a_P,	   U_a_P,      U_a_P,	   U_a_P,	U_a_P,	      U_a_P,	  U_a_P,      U_a_vXYpOFFSET,  U_a_P,
/* C */ xy_illegal,	 xy_illegal,	  xy_illegal,	   xy_illegal,	    xy_illegal,	     xy_illegal,      xy_illegal,	  xy_illegal,	   xy_illegal, xy_illegal, xy_illegal,	xy_cb_prefix, xy_illegal, xy_illegal, xy_illegal,      xy_illegal,
/* D */ xy_illegal,	 xy_illegal,	  xy_illegal,	   xy_illegal,	    xy_illegal,	     xy_illegal,      xy_illegal,	  xy_illegal,	   xy_illegal, xy_illegal, xy_illegal,	xy_illegal,   xy_illegal, xy_xy,      xy_illegal,      xy_illegal,
/* E */ xy_illegal,	 pop_XY,	  xy_illegal,	   ex_vsp_XY,	    xy_illegal,	     push_XY,	      xy_illegal,	  xy_illegal,	   xy_illegal, jp_XY,	   xy_illegal,	xy_illegal,   xy_illegal, xy_illegal, xy_illegal,      xy_illegal,
/* F */ xy_illegal,	 xy_illegal,	  xy_illegal,	   xy_illegal,	    xy_illegal,	     xy_illegal,      xy_illegal,	  xy_illegal,	   xy_illegal, ld_sp_XY,   xy_illegal,	xy_illegal,   xy_illegal, xy_xy,      xy_illegal,      xy_illegal
};

static Instruction const xy_cb_instruction_table[256] = {
/*	0		  1		    2		      3			4		  5		    6		      7			8		  9		    A		      B			C		  D		    E		      F */
/* 0 */ G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET_K,   G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET,     G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET_K,   G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET,     G_vXYpOFFSET_K,
/* 1 */ G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET_K,   G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET,     G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET_K,   G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET,     G_vXYpOFFSET_K,
/* 2 */ G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET_K,   G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET,     G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET_K,   G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET,     G_vXYpOFFSET_K,
/* 3 */ G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET_K,   G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET,     G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET_K,   G_vXYpOFFSET_K,	G_vXYpOFFSET_K,	  G_vXYpOFFSET_K,   G_vXYpOFFSET,     G_vXYpOFFSET_K,
/* 4 */ bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET,
/* 5 */ bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET,
/* 6 */ bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET,
/* 7 */ bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET, bit_N_vXYpOFFSET,
/* 8 */ M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K,
/* 9 */ M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K,
/* A */ M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K,
/* B */ M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K,
/* C */ M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K,
/* D */ M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K,
/* E */ M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K,
/* F */ M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET_K, M_N_vXYpOFFSET,   M_N_vXYpOFFSET_K
};


/* MARK: - Instructions: Prefix Handling */

INSTRUCTION(cb_prefix)
	{
	R++;
	return cb_instruction_table[DATA[1] = FETCH_OPCODE((PC += 2) - 1)](self);
	}


INSTRUCTION(ed_prefix)
	{
	R++;
	return ed_instruction_table[DATA[1] = FETCH_OPCODE(PC + 1)](self);
	}


#define XY_PREFIX(register)						     \
	zuint8 cycles;							     \
									     \
	if ((self->cycles += 4) >= self->cycle_limit)			     \
		{							     \
		RESUME = Z80_RESUME_XY;					     \
		return 0;						     \
		}							     \
									     \
	R++;								     \
	XY = register;							     \
	cycles = xy_instruction_table[DATA[1] = FETCH_OPCODE(PC + 1)](self); \
	register = XY;							     \
	return cycles;


INSTRUCTION(dd_prefix) {XY_PREFIX(IX)}
INSTRUCTION(fd_prefix) {XY_PREFIX(IY)}


/*------------------------------------------------------------------------.
| Instructions with DDCBh or FDCBh prefix increment R by 2, as only the   |
| bytes of the prefix are fetched by opcode fetch operations (M1 cycles). |
| The remaining 2 bytes are fetched by normal memory read operations.	  |
'========================================================================*/

INSTRUCTION(xy_cb_prefix)
	{
	FETCH_XY_EA((PC += 4) - 2);
	return xy_cb_instruction_table[DATA[3] = FETCH(PC - 1)](self);
	}


/*-----------------------------------------------------------------------------.
| In a sequence of DDh and/or FDh prefixes, it is the last one that counts, as |
| each prefix disables and replaces the previous one. No matter how long the   |
| sequence is, interrupts can only be responded after all prefixes are fetched |
| and the final instruction is executed. Each prefix consumes 4 T-states.      |
'=============================================================================*/

INSTRUCTION(xy_xy)
	{
	zuint8 cycles;
	zuint8 first_prefix = DATA[0];

	do	{
		PC++;
		DATA[0] = DATA[1];

		if ((self->cycles += 4) >= self->cycle_limit)
			{
			RESUME = Z80_RESUME_XY;
			return 0;
			}

		R++;
		}
	while (IS_XY_PREFIX(DATA[1] = FETCH_OPCODE(PC + 1)));

	if (DATA[0] == first_prefix) return xy_instruction_table[DATA[1]](self);

	if (first_prefix == 0xFD)
		{
		XY = IX;
		cycles = xy_instruction_table[DATA[1]](self);
		IX = XY;
		XY = IY;
		}

	else	{
		XY = IY;
		cycles = xy_instruction_table[DATA[1]](self);
		IY = XY;
		XY = IX;
		}

	return cycles;
	}


/* MARK: - Instructions: Illegal */

/*------------------------------------------------------------------.
| The CPU ignores illegal instructions with EDh prefix; in practice |
| they are all equivalent to two `nop` instructions (8 T-states).   |
'==================================================================*/

INSTRUCTION(ed_illegal)
	{
	if (self->illegal != Z_NULL) return self->illegal(CONTEXT, DATA[0]);
	Q_0
	PC += 2;
	return 8;
	}


/*------------------------------------------------------------------------.
| Illegal instructions with DDh or FDh prefix cause the CPU to ignore the |
| prefix, i.e., the byte immediately following the prefix is interpreted  |
| as the first byte of a new instruction. The prefix consumes 4 T-states. |
'========================================================================*/

INSTRUCTION(xy_illegal)
	{
	PC++;
	return instruction_table[DATA[0] = DATA[1]](self);
	}


/* MARK: - Instructions: Hooking */

INSTRUCTION(hook)
	{
	if (self->hook == Z_NULL)
		{
		Q_0
		PC++;
		return 4;
		}

	if ((DATA[0] = self->hook(CONTEXT, PC)) != Z80_HOOK)
		return instruction_table[DATA[0]](self);

	R--;
	return 0;
	}


/* MARK: - Interrupt Mode 0: PC Decrements for Unprefixed Instructions */

#ifdef Z80_WITH_FULL_IM0
	static zuint8 const im0_pc_decrement_table[256] = {
	/*      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
	/* 0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1 */ 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,
	/* 2 */ 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,
	/* 3 */ 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,
	/* 4 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 5 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 6 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 7 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 8 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 9 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* A */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* B */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* C */ 1, 0, 3, 0, 3, 0, 0, 1, 1, 0, 3, 0, 3, 3, 0, 1,
	/* D */ 1, 0, 3, 0, 3, 0, 0, 1, 1, 0, 3, 0, 3, 0, 0, 1,
	/* E */ 1, 0, 3, 0, 3, 0, 0, 1, 1, 0, 3, 0, 3, 0, 0, 1,
	/* F */ 1, 0, 3, 0, 3, 0, 0, 1, 1, 0, 3, 0, 3, 0, 0, 1
	};
#endif


/* MARK: - Interrupt Mode 0: Callback Trampolines */

#ifdef Z80_WITH_FULL_IM0
	static zuint8 im0_fetch(IM0 *self, zuint16 address)
		{
		Z_UNUSED(address)
		return self->z80->int_fetch(CONTEXT, self->pc);
		}


	static zuint8 im0_read(IM0 *self, zuint16 address)
		{return READ(address);}


	static void im0_write(IM0 *self, zuint16 address, zuint8 value)
		{WRITE(address, value);}


	static zuint8 im0_in(IM0 *self, zuint16 port)
		{return IN(port);}


	static void im0_out(IM0 *self, zuint16 port, zuint8 value)
		{OUT(port, value);}


	static void im0_ld_i_a(IM0 *self) {NOTIFY(ld_i_a);}
	static void im0_ld_r_a(IM0 *self) {NOTIFY(ld_r_a);}
	static void im0_reti  (IM0 *self) {NOTIFY(reti	);}
	static void im0_retn  (IM0 *self) {NOTIFY(retn	);}
#endif


/* MARK: - Public Functions */

/*----------------------------------------------------------------------.
| On POWER-ON, the CPU zeroes PC, I, and R, sets SP, IX, IY, AF, BC,	|
| DE, HL, AF', BC', DE' and HL' to to FFFFh [1,2], resets the interrupt |
| enable flip-flops (IFF1 and IFF2) and selects interrupt mode 0 [3].	|
| On Zilog NMOS models, F is sometimes set to FDh (NF reset) [1].	|
|									|
| There is no information about the initial state of MEMPTR and Q, so	|
| they are assumed to be 0.						|
|									|
| References:								|
| 1. https://baltazarstudios.com/webshare/Z80-undocumented-behavior.htm |
| 2. https://worldofspectrum.org/forums/discussion/34574		|
| 3. Young, Sean (2005-09-18). "Undocumented Z80 Documented, The"	|
|    v0.91, p. 20.							|
'======================================================================*/

Z80_API void z80_power(Z80 *self, zboolean state)
	{
	MEMPTR = PC = R = I = IFF1 = IFF2 = IM = Q =
	DATA[0] = HALT_LINE = INT_LINE = RESUME = REQUEST = 0;

	SP = IX = IY = AF = BC = DE = HL = AF_ = BC_ = DE_ = HL_ =
		state ? Z_UINT16(0xFFFF) : 0;
	}


/*--------------------------------------------------------------------------.
| The normal RESET zeroes PC, I, and R [1,2,3,4,5,6], resets the interrupt  |
| enable flip-flops (IFF1 and IFF2) [1,2,3,4,5] and selects the interrupt   |
| mode 0 [1,2,3,4,7].							    |
|									    |
| References:								    |
| 1. Zilog (2016-09). "Z80 CPU User Manual" revision 11, p. 6.		    |
| 2. SGS-Thomson (1990-01). "Z80 Microprocessor Family" 1st edition, p. 33. |
| 3. Brewer, Tony (2014-12). "Z80 Special Reset".			    |
|     * http://primrosebank.net/computers/z80/z80_special_reset.htm	    |
| 4. Flammenkamp, Achim. "Interrupt Behaviour of the Z80 CPU".		    |
|     * http://z80.info/interrup.htm					    |
| 5. https://baltazarstudios.com/webshare/Z80-undocumented-behavior.htm	    |
| 6. https://worldofspectrum.org/forums/discussion/34574		    |
| 7. Zilog (1978-05). "Z80 Family Program Interrupt Structure, The", p. 8.  |
'==========================================================================*/

Z80_API void z80_instant_reset(Z80 *self)
	{
	if (HALT_LINE) {SET_HALT_LINE(FALSE);}

	PC = R = I = IFF1 = IFF2 = IM =
	DATA[0] = HALT_LINE = RESUME = REQUEST = 0;
	}


#ifdef Z80_WITH_SPECIAL_RESET
	Z80_API void z80_special_reset(Z80 *self)
		{REQUEST |= Z80_REQUEST_SPECIAL_RESET;}
#endif


Z80_API void z80_int(Z80 *self, zboolean state)
	{
	if (!(INT_LINE = state)) REQUEST &= ~(zuint8)Z80_REQUEST_INT;
	else if (IFF1) REQUEST |= Z80_REQUEST_INT;
	}


Z80_API void z80_nmi(Z80 *self)
	{REQUEST |= Z80_REQUEST_NMI;}


#ifdef Z80_WITH_EXECUTE
	Z80_API zusize z80_execute(Z80 *self, zusize cycles)
		{
		ZInt16 *xy;

		R7		  = R;
		self->cycles	  = 0;
		self->cycle_limit = cycles;

		if (RESUME && cycles) switch (RESUME)
			{
			case Z80_RESUME_HALT:
			(void)halt(self);
			break;

			case Z80_RESUME_XY:
			RESUME = FALSE;
			R++; /* M1 */
			XY = (xy = &self->ix_iy[(DATA[0] >> 5) & 1])->uint16_value;
			self->cycles += xy_instruction_table[DATA[1] = FETCH_OPCODE(PC + 1)](self);
			xy->uint16_value = XY;
			break;
			}

		while (self->cycles < cycles)
			{
			R++; /* M1 */
			self->cycles += instruction_table[DATA[0] = FETCH_OPCODE(PC)](self);
			}

		R = R_ALL;	     /* restore R7 bit         */
		return self->cycles; /* return consumed cycles */
		}
#endif


Z80_API zusize z80_run(Z80 *self, zusize cycles)
	{
	ZInt16 *xy;
	zuint8 ird;

	/*---------------------------------------------------------------------.
	| The CPU increments R during each M1 cycle without altering the most  |
	| significant bit, commonly known as R7. This behavior is not emulated |
	| in every increment for obvious speed reasons. Instead, a copy of R   |
	| is used to preserve R7, which is restored before returning from this |
	| function. The emulation of `ld {a,r|r,a}` takes this into account.   |
	'=====================================================================*/
	R7 = R;

	self->cycles	  = 0;
	self->cycle_limit = cycles;

	if (RESUME && cycles) switch (RESUME)
		{
		/*----------------------------------------------------------------.
		| The CPU is halted. In order to avoid affecting the speed of the |
		| main execution loop, this state is executed by a dedicated loop |
		| within the function that emulates the `halt` instruction.	  |
		'================================================================*/
		case Z80_RESUME_HALT:
		if (REQUEST)
			{
			RESUME = FALSE;

#			ifdef Z80_WITH_SPECIAL_RESET
				if ((REQUEST & Z80_REQUEST_SPECIAL_RESET) && HALT_LINE)
					{
					zuint8 opcode;

					HALT_LINE = FALSE;

					if (self->halt != Z_NULL)
						self->halt(CONTEXT, Z80_HALT_EARLY_EXIT);

					if (IS_XY_PREFIX(DATA[0] = opcode = DATA[2]))
						self->cycles += instruction_table[FETCH_OPCODE(PC)](self);

					else if (opcode != 0x76)
						{
						PC--;
						self->cycles += instruction_table[opcode](self) - 4;
						}
					}
#			endif
			}

		else (void)halt(self);
		break;

		/*--------------------------------------------------------------.
		| The CPU is in normal operation state; the emulator ran out of |
		| clock cycles by fetching a DDh or FDh prefix.			|
		'==============================================================*/
		case Z80_RESUME_XY:
		RESUME = FALSE;
		R++;
		XY = (xy = &self->ix_iy[(DATA[0] >> 5) & 1])->uint16_value;
		self->cycles += xy_instruction_table[DATA[1] = FETCH_OPCODE(PC + 1)](self);
		xy->uint16_value = XY;
		break;

		/*----------------------------------------------------------------.
		| The CPU is responding to an INT in mode 0; the emulator ran out |
		| clock cycles by fetching a DDh or FDh prefix.			  |
		'================================================================*/
#		ifdef Z80_WITH_FULL_IM0
			case Z80_RESUME_IM0_XY:
			ird = DATA[0];
			goto im0_begin;
#		endif
		}

	while (self->cycles < cycles) /* main execution loop */
		{
		if (REQUEST)
			{
			/*-------------------------------------------------------------------------.
			| After detecting a special RESET signal, the CPU completes the ongoing	   |
			| instruction or interrupt response and then zeroes PC during the falling  |
			| edge of the next M1T1. The special RESET can be used in conjunction with |
			| an interrupt, in which case PC is zeroed during the subsequent interrupt |
			| acknowledge M-cycle. Otherwise, if no interrupt has been accepted at TL  |
			| of the instruction or interrupt response in which the special RESET has  |
			| been detected, the CPU produces an M1 cycle of 4 T-states to allow for   |
			| the fetch-execute overlap to take place, during which it fetches the	   |
			| next opcode and zeroes PC.						   |
			|									   |
			| References:								   |
			| * Brewer, Tony (2014-12). "Z80 Special Reset".			   |
			|     * http://primrosebank.net/computers/z80/z80_special_reset.htm	   |
			| * US Patent 4486827							   |
			| * Checked with "Visual Z80 Remix".					   |
			'=========================================================================*/
#			ifdef Z80_WITH_SPECIAL_RESET
				zuint8 special_reset = REQUEST & Z80_REQUEST_SPECIAL_RESET;
#			endif

			/*-------------------------------------------------------------------------.
			| NMI Response: Execute `rst 66h`			| T-states: 11:533 |
			|--------------------------------------------------------------------------|
			| The non-maskable interrupt takes priority over the maskable interrupt	   |
			| and cannot be disabled under software control. Its usual function is to  |
			| provide immediate response to important signals. The CPU responds to an  |
			| NMI by storing PC on the stack and jumping to the ISR located at address |
			| 0066h. The interrupt enable flip-flop 1 (IFF1) is reset to prevent any   |
			| INT from being accepted during the execution of this routine, which is   |
			| usually exited by using a `reti` or `retn` instruction to restore the	   |
			| original state of IFF1 [1].						   |
			|									   |
			| Some technical documents from Zilog include an erroneous timing diagram  |
			| showing an NMI acknowledge cycle of 4 T-states. However, documents from  |
			| other manufacturers and third parties specify that this M-cycle has 5	   |
			| T-states, as has been confirmed by low-level tests [2] and electronic	   |
			| simulations [3].							   |
			|									   |
			| The CPU does not accept a second NMI during the NMI response. Therefore, |
			| it is not possible to chain two NMI responses in a row without executing |
			| at least one instruction between them [3,4,5].			   |
			|									   |
			| References:								   |
			| 1. Zilog (1978-05). "Z80 Family Program Interrupt Structure, The",	   |
			|    pp. 4,5.								   |
			| 2. https://baltazarstudios.com/webshare/Z80-undocumented-behavior.htm	   |
			| 3. Checked with "Visual Z80 Remix".					   |
			| 4. https://spectrumcomputing.co.uk/forums/viewtopic.php?t=7086	   |
			| 5. https://stardot.org.uk/forums/viewtopic.php?t=24662		   |
			'=========================================================================*/
			if (REQUEST & Z80_REQUEST_REJECT_NMI)
				REQUEST = 0;

			else if (REQUEST & Z80_REQUEST_NMI)
				{
				REQUEST = Z80_REQUEST_REJECT_NMI;
				IFF1 = 0;
				if (HALT_LINE) {SET_HALT_LINE(FALSE);}
				R++; /* M1 */
				if (self->nmia != Z_NULL) (void)self->nmia(CONTEXT, PC);
				DATA[0] = 0;
				Q_0

#				ifdef Z80_WITH_SPECIAL_RESET
					PUSH(PC >> special_reset);
#				else
					PUSH(PC);
#				endif

				PC = MEMPTR = 0x66;
				self->cycles += 11;
				continue;
				}

			/*-------------------------------------------------------------------------.
			| INT Response								   |
			|--------------------------------------------------------------------------|
			| The maskable interrupt can be enabled and disabled by using the `ei` and |
			| `di` instructions respectively, which control the state of the interrupt |
			| enable flip-flops (IFF1 and IFF2). The CPU does not accept this kind of  |
			| interrupt directly after an `ei` instruction, but only after the one	   |
			| following `ei` is executed. This is so that ISRs can return without the  |
			| danger of being interrupted immediately after re-enabling interrupts if  |
			| the /INT line is still active, which could cause a stack overflow.	   |
			|									   |
			| As in `ei`, all forms of `reti` and `retn` defer the acceptance of the   |
			| maskable interrupt for one instruction, but this only occurs when IFF1   |
			| and IFF2 do not have the same state prior to the execution of either of  |
			| these instructions, which can only be caused by an earlier NMI response  |
			| [1,2,3].								   |
			|									   |
			| References:								   |
			| 1. https://floooh.github.io/2021/12/17/cycle-stepped-z80.html		   |
			| 2. https://spectrumcomputing.co.uk/forums/viewtopic.php?t=7086	   |
			| 3. https://stardot.org.uk/forums/viewtopic.php?t=24662		   |
			'=========================================================================*/
			else if (
#				ifdef Z80_WITH_SPECIAL_RESET
					(REQUEST & Z80_REQUEST_INT) &&
#				endif
				/* if the previous instruction is not `ei` */
				DATA[0] != 0xFB &&
				/* if the previous instruction is not `reti/retn` or IFF1 has not changed */
				(self->data.uint32_value & Z_UINT32_BIG_ENDIAN(Z_UINT32(0xFFC7FF00)))
				!=			   Z_UINT32_BIG_ENDIAN(Z_UINT32(0xED450000))
			)
				{
#				ifdef Z80_WITH_FULL_IM0
					Z80Read hook;
					IM0 im0;
#				endif

				REQUEST = IFF1 = IFF2 = 0;
				if (HALT_LINE) {SET_HALT_LINE(FALSE);}

				/*-------------------------------------------------------------------.
				| Due to a bug, the Zilog Z80 NMOS resets PF when an INT is accepted |
				| during the execution of the `ld a,{i|r}` instructions.	     |
				'===================================================================*/
#				ifdef Z80_WITH_ZILOG_NMOS_LD_A_IR_BUG
					if (	(OPTIONS & Z80_OPTION_LD_A_IR_BUG) &&
						(self->data.uint16_array[0] & Z_UINT16_BIG_ENDIAN(Z_UINT16(0xFFF7)))
						==			      Z_UINT16_BIG_ENDIAN(Z_UINT16(0xED57))
					)
						FLAGS = F & ~(zuint8)PF;
#				endif

				/*---------------------------------------------------------------------.
				| The INT acknowledge cycle (INTA) indicates that the interrupting I/O |
				| device can write to the data bus. 2 wait T-states are automatically  |
				| added to this M-cycle, allowing sufficient time to identify which    |
				| device must insert the interrupt response data (IRD). The first and  |
				| posibly sole byte of the IRD is read from the data bus during this   |
				| special M1 cycle.						       |
				|								       |
				| The value FFh is assumed when the `inta` callback is not used. This  |
				| is the most desirable behavior, since the `rst 38h` instruction will |
				| be executed if the interrupt mode is 0.			       |
				'=====================================================================*/
				R++; /* M1 */
				ird = (self->inta != Z_NULL) ? INTA : 0xFF;

#				ifdef Z80_WITH_SPECIAL_RESET
					PC >>= special_reset;
#				endif

				switch (IM) /* response */
					{
					/*-------------------------------------------------------------------------.
					| Interrupt Mode 0: Execute Instruction	     | T-states: 2*n + instruction |
					|--------------------------------------------------------------------------|
					| An instruction supplied via the data bus is executed. Its first byte is  |
					| read during the INT acknowledge cycle (INTA). If it is an opcode prefix, |
					| additional M-cycles of this kind are produced until the final opcode of  |
					| the instruction is fetched [1]. Each INT acknowledge cycle consumes as   |
					| many T-states as its normal M1 counterpart (the opcode fech M-cycle)     |
					| plus the 2 wait T-states mentioned above [1]. Subsequent bytes of the    |
					| instruction are fetched by using normal memory read M-cycles [1,2],      |
					| during which the interrupting I/O device must still supply the data [2]. |
					| The PC register, however, remains at its pre-interrupt state, not being  |
					| incremented as a result of the instruction fetch [1,2].		   |
					|									   |
					| References:								   |
					| 1. Checked with "Visual Z80 Remix".					   |
					| 2. Zilog (1978-05). "Z80 Family Program Interrupt Structure, The",	   |
					|    pp. 6,8.								   |
					'=========================================================================*/
					case 0:
					DATA[0] = ird;

#					ifdef Z80_WITH_FULL_IM0
						im0_begin:
						hook	      = self->hook;
						im0.z80	      = self;
						im0.context   = self->context;
						im0.fetch     = self->fetch;
						im0.read      = self->read;
						im0.write     = self->write;
						im0.in	      = self->in;
						im0.out	      = self->out;
						im0.pc	      = PC;
						self->context = &im0;
						self->fetch   = (Z80Read )im0_fetch;
						self->read    = (Z80Read )im0_read;
						self->write   = (Z80Write)im0_write;
						self->in      = (Z80Read )im0_in;
						self->out     = (Z80Write)im0_out;
						self->hook    = Z_NULL;

						im0_execute:

						if (im0_pc_decrement_table[ird])
							{
							PC -= im0_pc_decrement_table[ird];
							self->cycles += 2 + instruction_table[ird](self);
							}

						/* halt */
						else if (ird == 0x76) HALT_LINE = TRUE;

						/* instructions with CBh prefix */
						else if (ird == 0xCB)
							{
							R++;
							self->cycles += 4 + cb_instruction_table[DATA[1] = INTA](self);
							}

						/* instructions with EDh prefix */
						else if (ird == 0xED)
							{
							Instruction instruction;

							R++;

							if ((instruction = ed_instruction_table[DATA[1] = ird = INTA]) != ed_illegal)
								{
								im0.ld_i_a   = self->ld_i_a;
								im0.ld_r_a   = self->ld_r_a;
								im0.reti     = self->reti;
								im0.retn     = self->retn;
								self->ld_i_a = (Z80Notify)im0_ld_i_a;
								self->ld_r_a = (Z80Notify)im0_ld_r_a;
								self->reti   = (Z80Notify)im0_reti;
								self->retn   = (Z80Notify)im0_retn;

								self->cycles += 4 + instruction(self);

								self->ld_i_a = im0.ld_i_a;
								self->ld_r_a = im0.ld_r_a;
								self->reti   = im0.reti;
								self->retn   = im0.retn;

								/* All except: reti / retn */
								if ((ird & 0xC7) != 0x45) PC -= ((ird & 0xC7) == 0x43)
									? 4 /* ld SS,(WORD) / ld (WORD),SS */
									: 2 /* all others		   */;
								}

							else self->cycles += (self->illegal == Z_NULL)
								? 4 + 8
								: 4 + self->illegal(CONTEXT, ird);
							}

						/* instructions with DDh, FDh, DDCBh or FDCBh prefix */
						else if (IS_XY_PREFIX(ird))
							{
							Instruction instruction;

							if (RESUME) RESUME = FALSE;

							else	{
								im0_advance_xy:
								if ((self->cycles += 6) >= self->cycle_limit)
									{
									RESUME = Z80_RESUME_IM0_XY;
									goto im0_finalize;
									}
								}

							R++;

							if (IS_XY_PREFIX(ird = INTA))
								{
								DATA[0] = ird;
								goto im0_advance_xy;
								}

							if ((instruction = xy_instruction_table[ird]) == xy_illegal)
								{
								DATA[0] = ird;
								PC++;
								goto im0_execute;
								}

							XY = (xy = &self->ix_iy[((DATA[1] = ird) >> 5) & 1])->uint16_value;
							self->cycles += 2 + instruction(self);
							xy->uint16_value = XY;

							/* all except: jp (XY) */
							if (ird != 0xE9) PC = im0.pc;
							}

						else	{
							cycles += 2 + instruction_table[ird](self);

							/* all except: jp WORD / jp (hl)> / ret */
							if (ird != 0xC3 && (ird & 0xDF) != 0xC9) PC = im0.pc;
							}

						im0_finalize:
						self->context = im0.context;
						self->fetch   = im0.fetch;
						self->read    = im0.read;
						self->write   = im0.write;
						self->in      = im0.in;
						self->out     = im0.out;
						self->hook    = hook;

						if (HALT_LINE)
							{
							if (self->halt != Z_NULL) self->halt(im0.context, TRUE);
							RESUME = Z80_RESUME_HALT;
							Q_0
							self->cycles += 6;
							(void)halt(self);
							}

						continue;

#					else
						switch (ird)
							{
							case 0xC3: /* jp WORD */
							Q_0
							MEMPTR = PC = int_fetch_16(self);
							self->cycles += 2 + 10;
							continue;

							case 0xCD: /* call WORD */
							Q_0
							MEMPTR = int_fetch_16(self);
							PUSH(PC);
							PC = MEMPTR;
							self->cycles += 2 + 17;
							continue;

							default: /* `rst N` is assumed for other instructions */
							Q_0
							PUSH(PC);
							MEMPTR = PC = ird & 56;
							self->cycles += 2 + 11;
							continue;
							}
#					endif

					/*----------------------------------------------------------.
					| Interrupt Mode 1: Execute `rst 38h`	 | T-states: 13:733 |
					|-----------------------------------------------------------|
					| An internal `rst 38h` is executed. The interrupt response |
					| data read from the data bus is disregarded.		    |
					'==========================================================*/
					case 1:
					DATA[0] = 0;
					Q_0
					PUSH(PC);
					MEMPTR = PC = 0x38;
					self->cycles += 13;
					continue;

					/*---------------------------------------------------------------------.
					| Interrupt Mode 2: Execute `call (i:BYTE)`	  | T-states: 19:73333 |
					|----------------------------------------------------------------------|
					| An indirect call is executed. The pointer to the ISR is loaded from  |
					| the memory address formed by taking the I register as the most       |
					| significant byte, and the interrupt response vector read from the    |
					| data bus as the least significant byte.			       |
					|								       |
					| Zilog's official documentation states that the least significant bit |
					| of the interrupt response vector "must be a zero", since the address |
					| formed "is used to get two adjacent bytes to form a complete 16-bit  |
					| service routine starting address and the addresses must always start |
					| in even locations" [1]. However, Sean Young's experiments confirmed  |
					| that there is no such limitation [2]; any vector works whether it is |
					| odd or even.							       |
					|								       |
					| References:							       |
					| 1. Zilog (2005-03). "Z80 CPU User Manual" revision 5, pp. 25,26.     |
					| 2. Young, Sean (2005-09-18). "Undocumented Z80 Documented, The"      |
					|    v0.91, p. 20.						       |
					'=====================================================================*/
					case 2:
					DATA[0] = 0;
					Q_0
					PUSH(PC);
					MEMPTR = PC = READ_16((zuint16)(((zuint16)I << 8) | ird));
					self->cycles += 19;
					continue;
					}
				}

#			ifdef Z80_WITH_SPECIAL_RESET
				if (special_reset)
					{
					REQUEST = 0;

					/*-------------------------------------------------------------------.
					| The /HALT line quickly goes low and then high in TL when a special |
					| RESET is detected during the execution of the `halt` instruction.  |
					'===================================================================*/
					if (DATA[0] == 0x76 && self->halt != Z_NULL)
						self->halt(CONTEXT, Z80_HALT_CANCEL);

					R++; /* M1 */
					if (self->nop != Z_NULL) (void)self->nop(CONTEXT, PC);
					DATA[0] = 0;
					PC = 0;
					self->cycles += 4;
					continue;
					}
#			endif
			}

		R++; /* M1 */
		self->cycles += instruction_table[DATA[0] = FETCH_OPCODE(PC)](self);
		}

	R = R_ALL;	     /* restore R7 bit 	       */
	return self->cycles; /* return consumed cycles */
	}


#ifdef Z80_WITH_WINDOWS_DLL_MAIN
	int Z_MICROSOFT_STD_CALL _DllMainCRTStartup(void *hDllHandle, unsigned long dwReason, void *lpReserved)
		{return TRUE;}
#endif


/* Z80.c EOF */
