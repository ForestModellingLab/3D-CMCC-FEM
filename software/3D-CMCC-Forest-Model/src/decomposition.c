/*
 * decomposition.c
 *
 *  Created on: 13 mar 2017
 *      Author: alessio
 */


/* includes */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "matrix.h"
#include "common.h"
#include "constants.h"
#include "settings.h"
#include "logger.h"


void decomposition (cell_t *const c, const meteo_daily_t *const meteo_daily)
{
	double soil_tempK;                           /* soil temperature (Kelvin) */
	double minpsi, maxpsi;                       /* minimum and maximum soil water potential limits (MPa) */

	double cn_l1, cn_l2, cn_l4, cn_s1, cn_s2, cn_s3, cn_s4;
	double rfl1s1, rfl2s2, rfl4s3, rfs1s2, rfs2s3, rfs3s4;
	double kl1, kl2, kl4;                                                         /* current decomposition rate constant litter */
	double ks1, ks2, ks3, ks4;                                                    /* current decomposition rate constant SOM */
	double kfrag;                                                                 /* current fragmentation rate constant CWD */
	double cwdc_loss;
	double plitr1c_loss, plitr2c_loss, plitr4c_loss;                              /* decomposition */
	double psoil1c_loss, psoil2c_loss, psoil3c_loss, psoil4c_loss;
	double pmnf_l1s1,pmnf_l2s2,pmnf_l4s3,pmnf_s1s2,pmnf_s2s3,pmnf_s3s4,pmnf_s4;
	double potential_immob,mineralized;
	int nlimit;
	double ratio;

	/** following BIOME-BGC decomp.c file **/
	/* calculate the rate constant scalar for soil temperature,
	assuming that the base rate constants are assigned for non-moisture
	limiting conditions at 25 C. The function used here is taken from
	Lloyd, J., and J.A. Taylor, 1994. On the temperature dependence of
	soil respiration. Functional Ecology, 8:315-323.
	This equation is a modification of their eqn. 11, changing the base
	temperature from 10 C to 25 C, since most of the microcosm studies
	used to get the base decomp rates were controlled at 25 C. */

	/* check for soil temperature */
	if ( meteo_daily->tsoil < -10. )
	{
		/* no decomposition processes for tsoil < -10.0 C */
		c->tsoil_scalar= 0.;
	}
	else
	{
		soil_tempK = meteo_daily->tsoil + 273.15;
		c->tsoil_scalar = exp(308.56*((1./71.02)-(1.0/(soil_tempK-227.13))));
	}

	/* calculate the rate constant scalar for soil water content.
	Uses the log relationship with water potential given in
	Andren, O., and K. Paustian, 1987. Barley straw decomposition in the field:
	a comparison of models. Ecology, 68(5):1190-1200.
	and supported by data in
	Orchard, V.A., and F.J. Cook, 1983. Relationship between soil respiration
	and soil moisture. Soil Biol. Biochem., 15(4):447-453.
	 */
	/* set the maximum and minimum values for water potential limits (MPa) */
	minpsi = -10.;
	maxpsi = c->psi_sat;

	/* check for soil water */
	if (c->psi < minpsi)
	{
		/* no decomposition below the minimum soil water potential */
		c->wsoil_scalar = 0.;
	}
	else if (c->psi > maxpsi)
	{
		/* this shouldn't ever happen, but just in case... */
		c->wsoil_scalar = 1.;
	}
	else
	{
		c->wsoil_scalar = log(minpsi/c->psi)/log(minpsi/maxpsi);
	}

	/* calculate the final rate scalar as the product of the temperature and water scalars */
	c->rate_scalar = c->tsoil_scalar * c->wsoil_scalar;

	/* calculate compartment C:N ratios */
	if (c->litr1N > 0.0) cn_l1 = c->litr1C/c->litr1N;
	if (c->litr2N > 0.0) cn_l2 = c->litr2C/c->litr2N;
	if (c->litr4N > 0.0) cn_l4 = c->litr4C/c->litr4N;

	cn_s1 = SOIL1_CN;
	cn_s2 = SOIL2_CN;
	cn_s3 = SOIL3_CN;
	cn_s4 = SOIL4_CN;

	/* respiration fractions for fluxes between compartments */
	rfl1s1 = RFL1S1;
	rfl2s2 = RFL2S2;
	rfl4s3 = RFL4S3;
	rfs1s2 = RFS1S2;
	rfs2s3 = RFS2S3;
	rfs3s4 = RFS3S4;

	/* calculate the corrected rate constants from the rate scalar and their
	base values. All rate constants are (1/day) */
	/* compute decomposition rates for each pool */
	kl1 = KL1_BASE * c->rate_scalar;                   /* labile litter pool */
	kl2 = KL2_BASE * c->rate_scalar;                   /* cellulose litter pool */
	kl4 = KL4_BASE * c->rate_scalar;                   /* lignin litter pool */
	ks1 = KS1_BASE * c->rate_scalar;                   /* fast microbial recycling pool */
	ks2 = KS2_BASE * c->rate_scalar;                   /* medium microbial recycling pool */
	ks3 = KS3_BASE * c->rate_scalar;                   /* slow microbial recycling pool */
	ks4 = KS4_BASE * c->rate_scalar;                   /* recalcitrant SOM (humus) pool */
	kfrag = KFRAG_BASE * c->rate_scalar;               /* physical fragmentation of coarse woody debris */

	/*note: model computes here for each single class each litter and soil pools class related */


	/* calculate the flux from CWD to litter lignin and cellulose
	compartments, due to physical fragmentation */
	//TODO
	/*
	cwdc_loss = kfrag * cs->cwdc;
	cf->cwdc_to_litr2c = cwdc_loss * epc->deadwood_fucel;
	cf->cwdc_to_litr3c = cwdc_loss * epc->deadwood_fscel;
	cf->cwdc_to_litr4c = cwdc_loss * epc->deadwood_flig;
	nf->cwdn_to_litr2n = cf->cwdc_to_litr2c/epc->deadwood_cn;
	nf->cwdn_to_litr3n = cf->cwdc_to_litr3c/epc->deadwood_cn;
	nf->cwdn_to_litr4n = cf->cwdc_to_litr4c/epc->deadwood_cn;
	 */

	/* initialize the potential loss and mineral N flux variables */
	plitr1c_loss = plitr2c_loss = plitr4c_loss = 0.0;
	psoil1c_loss = psoil2c_loss = psoil3c_loss = psoil4c_loss = 0.0;
	pmnf_l1s1 = pmnf_l2s2 = pmnf_l4s3 = 0.0;
	pmnf_s1s2 = pmnf_s2s3 = pmnf_s3s4 = pmnf_s4 = 0.0;


	/** decomposition **/
	/* calculate the non-nitrogen limited fluxes between litter and
	soil compartments. These will be ammended for N limitation if it turns
	out the potential gross immobilization is greater than potential gross
	mineralization */

	/* 1. labile litter to fast microbial recycling pool */
	if (c->litr1C > 0.0)
	{
		plitr1c_loss = kl1 * c->litr1C;
		if (c->litr1N > 0.0)
		{
			ratio = cn_s1/cn_l1;
		}
		else
		{
			ratio = 0.0;
		}
		pmnf_l1s1 = (plitr1c_loss * (1.0 - rfl1s1 - (ratio)))/cn_s1;
	}

	/* 2. cellulose litter to medium microbial recycling pool */
	if (c->litr2C > 0.0)
	{
		plitr2c_loss = kl2 * c->litr2C;
		if (c->litr2N > 0.0) ratio = cn_s2/cn_l2;
		else ratio = 0.0;
		pmnf_l2s2 = (plitr2c_loss * (1.0 - rfl2s2 - (ratio)))/cn_s2;
	}

	/* 3. lignin litter to slow microbial recycling pool */
	if (c->litr4C > 0.0)
	{
		plitr4c_loss = kl4 * c->litr4C;
		if (c->litr4N > 0.0) ratio = cn_s3/cn_l4;
		else ratio = 0.0;
		pmnf_l4s3 = (plitr4c_loss * (1.0 - rfl4s3 - (ratio)))/cn_s3;
	}

	//	/* 4. fast microbial recycling pool to medium microbial recycling pool */
	//	if (cs->soil1c > 0.0)
	//	{
	//		psoil1c_loss = ks1 * cs->soil1c;
	//		pmnf_s1s2 = (psoil1c_loss * (1.0 - rfs1s2 - (cn_s2/cn_s1)))/cn_s2;
	//	}
	//
	//	/* 5. medium microbial recycling pool to slow microbial recycling pool */
	//	if (cs->soil2c > 0.0)
	//	{
	//		psoil2c_loss = ks2 * cs->soil2c;
	//		pmnf_s2s3 = (psoil2c_loss * (1.0 - rfs2s3 - (cn_s3/cn_s2)))/cn_s3;
	//	}
	//
	//	/* 6. slow microbial recycling pool to recalcitrant SOM pool */
	//	if (cs->soil3c > 0.0)
	//	{
	//		psoil3c_loss = ks3 * cs->soil3c;
	//		pmnf_s3s4 = (psoil3c_loss * (1.0 - rfs3s4 - (cn_s4/cn_s3)))/cn_s4;
	//	}
	//
	//	/* 7. mineralization of recalcitrant SOM */
	//	if (cs->soil4c > 0.0)
	//	{
	//		psoil4c_loss = ks4 * cs->soil4c;
	//		pmnf_s4 = -psoil4c_loss/cn_s4;
	//	}

	/* determine if there is sufficient mineral N to support potential
	immobilization. Immobilization fluxes are positive, mineralization fluxes
	are negative */
	nlimit = 0;
	potential_immob = 0.0;
	mineralized = 0.0;
	if (pmnf_l1s1 > 0.0) potential_immob += pmnf_l1s1;
	else mineralized += -pmnf_l1s1;
	if (pmnf_l2s2 > 0.0) potential_immob += pmnf_l2s2;
	else mineralized += -pmnf_l2s2;
	if (pmnf_l4s3 > 0.0) potential_immob += pmnf_l4s3;
	else mineralized += -pmnf_l4s3;
	//	if (pmnf_s1s2 > 0.0) potential_immob += pmnf_s1s2;
	//	else mineralized += -pmnf_s1s2;
	//	if (pmnf_s2s3 > 0.0) potential_immob += pmnf_s2s3;
	//	else mineralized += -pmnf_s2s3;
	//	if (pmnf_s3s4 > 0.0) potential_immob += pmnf_s3s4;
	//	else mineralized += -pmnf_s3s4;
	//	mineralized += -pmnf_s4;

	/* save the potential fluxes until plant demand has been assessed,
	to allow competition between immobilization fluxes and plant growth
	demands */
	c->mineralized = mineralized;
	c->potential_immob = potential_immob;
	c->plitr1c_loss = plitr1c_loss;
	c->pmnf_l1s1 = pmnf_l1s1;
	c->plitr2c_loss = plitr2c_loss;
	c->pmnf_l2s2 = pmnf_l2s2;
	c->plitr4c_loss = plitr4c_loss;
	c->pmnf_l4s3 = pmnf_l4s3;
	c->psoil1c_loss = psoil1c_loss;
	c->pmnf_s1s2 = pmnf_s1s2;
	c->psoil2c_loss = psoil2c_loss;
	c->pmnf_s2s3 = pmnf_s2s3;
	c->psoil3c_loss = psoil3c_loss;
	c->pmnf_s3s4 = pmnf_s3s4;
	c->psoil4c_loss = psoil4c_loss;
	c->kl4 = kl4;

	/* store the day's gross mineralization */
	c->daily_gross_nmin = mineralized ;

}