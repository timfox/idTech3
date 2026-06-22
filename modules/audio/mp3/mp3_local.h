#pragma once

#include "l3.h"

int L3table_init(void);

float *quant_init_global_addr(void);
float (*quant_init_scale_addr(void))[4][32];
float *quant_init_pow_addr(void);
float *quant_init_subblock_addr(void);

float (*alias_init_addr(void))[2];
float (*msis_init_addr(void))[8][2];
float (*msis_init_addr_MPEG2(void))[2][64][2];

float (*hwin_init_addr(void))[36];
typedef struct {
	float *w;
	float *w2;
	void *coef;
} IMDCT_INIT_BLOCK;

void imdct18(float f[18]);
void imdct6_3(float f[]);
const IMDCT_INIT_BLOCK *imdct_init_addr_18(void);
const IMDCT_INIT_BLOCK *imdct_init_addr_6(void);

int hybrid(float xin[], float xprev[], float y[18][32], int btype, int nlong, int ntot, int nprev);
int hybrid_sum(float xin[], float xin_left[], float y[18][32], int btype, int nlong, int ntot);
void sum_f_bands(float a[], float b[], int n);
void FreqInvert(float y[18][32], int n);

void antialias(float x[], int n);
void ms_process(float x[][1152], int n);
void is_process_MPEG1(float x[][1152], SCALEFACT *sf, CB_INFO cb_info[2], int nsamp, int ms_mode);
void is_process_MPEG2(float x[][1152], SCALEFACT *sf, CB_INFO cb_info[2], IS_SF_INFO *is_sf_info, int nsamp, int ms_mode);

void unpack_huff(int xy[][2], int n, int ntable);
int unpack_huff_quad(int vwxy[][4], int n, int nbits, int ntable);
void dequant(SAMPLE sample[], int *nsamp, SCALEFACT *sf, GR *gr, CB_INFO *cb_info, int ncbl_mixed);
void unpack_sf_sub_MPEG1(SCALEFACT *scalefac, GR *gr, int scfsi, int igr);
void unpack_sf_sub_MPEG2(SCALEFACT sf[], GR *grdat, int is_and_ch, IS_SF_INFO *is_sf_info);
