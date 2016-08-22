/*
Copyright © 2011-2016, Grzegorz Kraszewski.
See LICENSE file.
*/

/*======================================================================*/
/* IMPORTANT                                                            */
/*                                                                      */
/* The code works in "round towards zero" mode. This is different from  */
/* GCC standard library strtod(), which uses "round half to even" rule. */
/* Therefore it cannot be used as a direct drop-in replacement, as in   */
/* some cases results will be different on the least significant bit of */
/* mantissa. Read more in the README.md file.                           */
/*======================================================================*/


#include <stdint.h>

#define DIGITS 18

#define DOUBLE_PLUS_ZERO          0x0000000000000000ULL
#define DOUBLE_MINUS_ZERO         0x8000000000000000ULL
#define DOUBLE_PLUS_INFINITY      0x7FF0000000000000ULL
#define DOUBLE_MINUS_INFINITY     0xFFF0000000000000ULL

union HexDouble
{
	double d;
	uint64_t u;
};


#define lsr96(s2, s1, s0, d2, d1, d0) \
d0 = ((s0) >> 1) | (((s1) & 1) << 31); \
d1 = ((s1) >> 1) | (((s2) & 1) << 31); \
d2 = (s2) >> 1;

#define lsl96(s2, s1, s0, d2, d1, d0) \
d2 = ((s2) << 1) | (((s1) & (1 << 31)) >> 31); \
d1 = ((s1) << 1) | (((s0) & (1 << 31)) >> 31); \
d0 = (s0) << 1;


/* 
Undefine the below constant if your processor or compiler is slow
at 64-bit arithmetic. This is a rare case however. 64-bit macros are
better for deeply pipelined CPUs (no conditional execution), are 
very efficient for 64-bit processors and also fast on 32-bit processors
featuring extended precision arithmetic (x86, PowerPC_32, M68k and probably
more).
*/

#define USE_64BIT_FOR_ADDSUB_MACROS 1

#ifdef USE_64BIT_FOR_ADDSUB_MACROS

#define add96(s2, s1, s0, d2, d1, d0) { \
uint64_t w; \
w = (uint64_t)(s0) + (uint64_t)(d0); \
(s0) = w; \
w >>= 32; \
w += (uint64_t)(s1) + (uint64_t)(d1); \
(s1) = w; \
w >>= 32; \
w += (uint64_t)(s2) + (uint64_t)(d2); \
(s2) = w; }

#define sub96(s2, s1, s0, d2, d1, d0) { \
uint64_t w; \
w = (uint64_t)(s0) - (uint64_t)(d0); \
(s0) = w; \
w >>= 32; \
w += (uint64_t)(s1) - (uint64_t)(d1); \
(s1) = w; \
w >>= 32; \
w += (uint64_t)(s2) - (uint64_t)(d2); \
(s2) = w; }

#else

#define add96(s2, s1, s0, d2, d1, d0) { \
uint32_t _x, _c; \
_x = (s0); (s0) += (d0); \
if ((s0) < _x) _c = 1; else _c = 0; \
_x = (s1); (s1) += (d1) + _c; \
if (((s1) < _x) || (((s1) == _x) && _c)) _c = 1; else _c = 0; \
(s2) += (d2) + _c; }

#define sub96(s2, s1, s0, d2, d1, d0) { \
uint32_t _x, _c; \
_x = (s0); (s0) -= (d0); \
if ((s0) > _x) _c = 1; else _c = 0; \
_x = (s1); (s1) -= (d1) + _c; \
if (((s1) > _x) || (((s1) == _x) && _c)) _c = 1; else _c = 0; \
(s2) -= (d2) + _c; }

#endif   /* USE_64BIT_FOR_ADDSUB_MACROS */



/* parser state machine states */

#define FSM_A     0
#define FSM_B     1
#define FSM_C     2
#define FSM_D     3
#define FSM_E     4
#define FSM_F     5
#define FSM_G     6
#define FSM_H     7
#define FSM_I     8
#define FSM_STOP  9


/* Modify these if working with non-ASCII encoding */

#define DPOINT '.'
#define ISDIGIT(x) (((x) >= '0') && ((x) <= '9'))
#define ISSPACE(x) ((((x) >= 0x09) && ((x) <= 0x13)) || ((x) == 0x20))
#define ISEXP(x) (((x) == 'E') || ((x) == 'e'))


/* The structure is filled by parser, then given to converter. */

struct PrepNumber
{
	int negative;                      /* 0 if positive number, 1 if negative */
	int32_t exponent;                  /* power of 10 exponent */
	uint64_t mantissa;                 /* integer mantissa */
};


/* Possible parser return values. */

#define PARSER_OK     0  // parser finished OK
#define PARSER_PZERO  1  // no digits or number is smaller than +-2^-1022
#define PARSER_MZERO  2  // number is negative, module smaller
#define PARSER_PINF   3  // number is higher than +HUGE_VAL
#define PARSER_MINF   4  // number is lower than -HUGE_VAL


/* GETC() macro gets next character from processed string. */

#define GETC(s) *s++


static int parser(char *s, struct PrepNumber *pn)
{
	int state = FSM_A;
	int digx = 0, c = ' ';            /* initial value for kicking off the state machine */
	int result = PARSER_OK;
	int expneg = 0;
	int32_t expexp = 0;

	while (state != FSM_STOP)
	{
		switch (state)
		{
			case FSM_A:
				if (ISSPACE(c)) c = GETC(s);
				else state = FSM_B;
			break;

			case FSM_B:
				state = FSM_C;

				if (c == '+') c = GETC(s);
				else if (c == '-')
				{
					pn->negative = 1;
					c = GETC(s);
				}
				else if (ISDIGIT(c)) {}
				else if (c == DPOINT) {}
				else state = FSM_STOP;
			break;

			case FSM_C:
				if (c == '0') c = GETC(s);
				else if (c == DPOINT)
				{
					c = GETC(s);
					state = FSM_D;
				}
				else state = FSM_E;
			break;

			case FSM_D:
				if (c == '0')
				{
					c = GETC(s);
					if (pn->exponent > -2147483647) pn->exponent--;
				}
				else state = FSM_F;
			break;

			case FSM_E:
				if (ISDIGIT(c))
				{
					if (digx < DIGITS)
					{
						pn->mantissa *= 10;
						pn->mantissa += c - '0';
						digx++;
					}
					else if (pn->exponent < 2147483647) pn->exponent++;

					c = GETC(s);
				}
				else if (c == DPOINT)
				{
					c = GETC(s);
					state = FSM_F;
				}
				else state = FSM_F;
			break;

			case FSM_F:
				if (ISDIGIT(c))
				{
					if (digx < DIGITS)
					{
						pn->mantissa *= 10;
						pn->mantissa += c - '0';
						pn->exponent--;
						digx++;
					}

					c = GETC(s);
				}
				else if (ISEXP(c))
				{
					c = GETC(s);
					state = FSM_G;
				}
				else state = FSM_G;
			break;

			case FSM_G:
				if (c == '+') c = GETC(s);
				else if (c == '-')
				{
					expneg = 1;
					c = GETC(s);
				}

				state = FSM_H;
			break;

			case FSM_H:
				if (c == '0') c = GETC(s);
				else state = FSM_I;
			break;

			case FSM_I:
				if (ISDIGIT(c))
				{
					if (expexp < 214748364)
					{
						expexp *= 10;
						expexp += c - '0';
					}

					c = GETC(s);
				}
				else state = FSM_STOP;
			break;
		}
	}

	if (expneg) expexp = -expexp;
	pn->exponent += expexp;

	if (pn->mantissa == 0)
	{
		if (pn->negative) result = PARSER_MZERO;
		else result = PARSER_PZERO;
	}
	else if (pn->exponent > 309)
	{
		if (pn->negative) result = PARSER_MINF;
		else result = PARSER_PINF;
	}
	else if (pn->exponent < -328)
	{
		if (pn->negative) result = PARSER_MZERO;
		else result = PARSER_PZERO;
	}

	return result;
}


static double converter(struct PrepNumber *pn)
{
	int binexp = 92;
	union HexDouble hd;
	uint32_t s2, s1, s0;      /* 96-bit precision integer */
	uint32_t q2, q1, q0;      /* 96-bit precision integer */
	uint32_t r2, r1, r0;      /* 96-bit precision integer */
	uint32_t mask28 = 0xF << 28;

	hd.u = 0;

	s0 = (uint32_t)(pn->mantissa & 0xFFFFFFFF);
	s1 = (uint32_t)(pn->mantissa >> 32);
	s2 = 0;

	while (pn->exponent > 0)
	{
		lsl96(s2, s1, s0, q2, q1, q0);   // q = p << 1
		lsl96(q2, q1, q0, r2, r1, r0);   // r = p << 2
		lsl96(r2, r1, r0, s2, s1, s0);   // p = p << 3
		add96(s2, s1, s0, q2, q1, q0);   // p = (p << 3) + (p << 1)

		pn->exponent--;

		while (s2 & mask28)
		{
			lsr96(s2, s1, s0, q2, q1, q0);
			binexp++;
			s2 = q2;
			s1 = q1;
			s0 = q0;
		}
	}

	while (pn->exponent < 0)
	{
		while (!(s2 & (1 << 31)))
		{
			lsl96(s2, s1, s0, q2, q1, q0);
			binexp--; 
			s2 = q2;
			s1 = q1;
			s0 = q0;
		}

		q2 = s2 / 10;
		r1 = s2 % 10;
		r2 = (s1 >> 8) | (r1 << 24);
		q1 = r2 / 10;
		r1 = r2 % 10;
		r2 = ((s1 & 0xFF) << 16) | (s0 >> 16) | (r1 << 24);
		r0 = r2 / 10;
		r1 = r2 % 10;
		q1 = (q1 << 8) | ((r0 & 0x00FF0000) >> 16);
		q0 = r0 << 16;
		r2 = (s0 & 0xFFFF) | (r1 << 16);
		q0 |= r2 / 10;
		s2 = q2;
		s1 = q1;
		s0 = q0;

		pn->exponent++;
	}

	if (s2 || s1 || s0)
	{
		while (!(s2 & mask28))
		{
			lsl96(s2, s1, s0, q2, q1, q0);
			binexp--;
			s2 = q2;
			s1 = q1;
			s0 = q0;
		}
	}

	binexp += 1023;

	if (binexp > 2046)
	{
		if (pn->negative) hd.u = DOUBLE_MINUS_INFINITY;
		else hd.u = DOUBLE_PLUS_INFINITY;
	}
	else if (binexp < 1)
	{
		if (pn->negative) hd.u = DOUBLE_MINUS_ZERO;
	}
	else if (s2)
	{
		uint64_t q;
		uint64_t binexs2 = (uint64_t)binexp;

		binexs2 <<= 52;
		q = ((uint64_t)(s2 & ~mask28) << 24) | (((uint64_t)s1 + 128) >> 8) | binexs2;
		if (pn->negative) q |= (1ULL << 63);
		hd.u = q;
	}

	return hd.d;
}


double str2dbl(char *s)
{
	struct PrepNumber pn;
	union HexDouble hd;
	int i;
	double result;

	pn.mantissa = 0;
	pn.negative = 0;
	pn.exponent = 0;
	hd.u = DOUBLE_PLUS_ZERO;

	i = parser(s, &pn);

	switch (i)
	{
		case PARSER_OK:
			result = converter(&pn);
		break;

		case PARSER_PZERO:
			result = hd.d;
		break;

		case PARSER_MZERO:
			hd.u = DOUBLE_MINUS_ZERO;
			result = hd.d;
		break;

		case PARSER_PINF:
			hd.u = DOUBLE_PLUS_INFINITY;
			result = hd.d;
		break;

		case PARSER_MINF:
			hd.u = DOUBLE_MINUS_INFINITY;
			result = hd.d;
		break;
	}

	return result;
}

