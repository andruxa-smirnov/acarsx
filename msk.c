/*
 * Copyright (c) 2014 Thierry Leconte (f4dwv)
 * Copyright (c) 2014 Jack Tan(BH1RBH) at Jack's Lab
 * 
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License version 2
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "acarsx.h"

#define DCCF 240.0

#define PLLKa 1.8991680918e+02
#define PLLKb 9.8503292076e-01
#define PLLKc 0.9995

int init_msk(channel_t * ch)
{
	int i;

	ch->MskFreq = 1800.0 / (float)(ch->Infs) * 2.0 * M_PI;
	ch->MskPhi = ch->MskClk = 0;
	ch->MskS = 0;

	ch->MskKa = PLLKa / (float)(ch->Infs);
	ch->MskDf = ch->Mska = 0;

	ch->Mskdc = 0;
	ch->Mskdcf = DCCF / (float)ch->Infs;

	ch->flen = (ch->Infs / 1200 + 0.5);
	ch->idx = 0;
	ch->I = calloc(ch->flen, sizeof(float));
	ch->Q = calloc(ch->flen, sizeof(float));
	ch->h = calloc(ch->flen, sizeof(float));

	for (i = 0; i < ch->flen; i++) {
		ch->h[i] = sinf(2 * M_PI * 600 / ch->Infs * (i + 1));
		ch->I[i] = ch->Q[i] = 0;
	}

	return 0;
}

static float fst_atan2(float y, float x)
{
	float r, angle;
	float abs_y = fabs(y) + 1e-10;	// kludge to prevent 0/0 condition
	if (x >= 0) {
		r = (x - abs_y) / (x + abs_y);
		angle = M_PI_4 - M_PI_4 * r;
	} else {
		r = (x + abs_y) / (abs_y - x);
		angle = 3 * M_PI_4 - M_PI_4 * r;
	}
	if (y < 0)
		return (-angle);	// negate if in quad III or IV
	else
		return (angle);
}

static void putbit(float v, channel_t * ch)
{
	ch->outbits >>= 1;
	if (v > 0) {
		ch->outbits |= 0x80;
	}
	ch->nbits--;
	if (ch->nbits <= 0)
		decode_acars(ch);
}

void demod_msk(float in, channel_t * ch)
{
	int idx, j;

	float iv, qv, s, bit;
	float dphi;
	float p, sp, cp;

	/* oscilator */
	p = ch->MskFreq + ch->MskDf;
	ch->MskClk += p;
	p = ch->MskPhi + p;
	if (p >= 2.0 * M_PI) {
		p -= 2.0 * M_PI;
	}
	ch->MskPhi = p;

	idx = ch->idx;

	if (ch->MskClk > 3 * M_PI / 2) {
		ch->MskClk -= 3 * M_PI / 2;

		/* matched filter */
		for (j = 0, iv = qv = 0; j < ch->flen - 1; j++) {
			int k = (idx + 1 + j) % ch->flen;
			iv += ch->h[j] * ch->I[k];
			qv += ch->h[j] * ch->Q[k];
		}

		if ((ch->MskS & 1) == 0) {
			if (iv >= 0)
				dphi = fst_atan2(-qv, iv);
			else
				dphi = fst_atan2(qv, -iv);
			if (ch->MskS & 2) {
				bit = iv;
			} else {
				bit = -iv;
			}
			putbit(bit, ch);
		} else {
			if (qv >= 0)
				dphi = fst_atan2(iv, qv);
			else
				dphi = fst_atan2(-iv, -qv);
			if (ch->MskS & 2) {
				bit = -qv;
			} else {
				bit = qv;
			}
			putbit(bit, ch);
		}
		ch->MskS = (ch->MskS + 1) & 3;

		/* PLL */
		dphi *= ch->MskKa;
		ch->MskDf = PLLKc * ch->MskDf + dphi - PLLKb * ch->Mska;
		ch->Mska = dphi;
	}

	/* DC blocking */
	s = in - ch->Mskdc;
	ch->Mskdc = (1.0 - ch->Mskdcf) * ch->Mskdc + ch->Mskdcf * in;

	/* FI */
	sincosf(p, &sp, &cp);
	ch->I[idx] = s * cp;
	ch->Q[idx] = s * sp;

	ch->idx = (idx + 1) % ch->flen;
}
