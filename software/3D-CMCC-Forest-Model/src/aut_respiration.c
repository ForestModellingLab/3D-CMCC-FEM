/*
 * autotrophic_respiration.c
 *
 *  Created on: 25/set/2013
 *      Author: alessio
 */

/* includes */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "aut_respiration.h"
#include "common.h"
#include "constants.h"
#include "settings.h"
#include "logger.h"

extern settings_t* g_settings;
extern logger_t* g_debug_log;

void maintenance_respiration(cell_t *const c, const int layer, const int height, const int dbh, const int age, const int species, const meteo_daily_t *const meteo_daily)
{
	double MR_ref     = 0.218;               /* Reference MR respiration linear N relationship with MR being kgC/kgN/day, 0.218 from Ryan 1991, 0.1584 Campioli et al., 2013 and from Dufrene et al 2005 */

	//NOTE: Atkin et al. 2008 and Cox et al., reports 25 °C for both
	double Q10_temp; //test 25.;     /* T_base temperature for respiration, 15°C for Damesin et al., 2001, 20°C Thornton */
	//double Q10_temp_accl = 20.0; //25;     /* T_base temperature for acclimation in respiration (25°C) Atkin et al., 2008 GCB, Cox et al., 2000 Nature */

	//note: q10 variables are recomputed by resp acclimation 'Type I'
	double q10_tavg   = 2.;                  /* fractional change in rate with a T 10 °C increase in temperature: 2.2 from Schwalm & Ek, 2004; Kimball et al., 1997, 1.5 Mahecha Science, 2010 */
	double q10_tday   = 2.;                  /* fractional change in rate with a T 10 °C increase in temperature: 2.2 from Schwalm & Ek, 2004; Kimball et al., 1997, 1.5 Mahecha Science, 2010 */
	double q10_tnight = 2.;                  /* fractional change in rate with a T 10 °C increase in temperature: 2.2 from Schwalm & Ek, 2004; Kimball et al., 1997, 1.5 Mahecha Science, 2010 */
	double q10_tsoil  = 2.;                  /* fractional change in rate with a T 10 °C increase in temperature: 2.2 from Schwalm & Ek, 2004; Kimball et al., 1997, 1.5 Mahecha Science, 2010 */

	double acc_const  = -0.00794; // -0.00703;             /* temperature correction factor for acclimation -0.00703 Atkin et al., 2008 GCB, -0.00794 Smith & Dukes 2012; 0.0078 Hidy et al., 2016 GMD */

	/* exponent for Temperature */
	double exponent_tday;
	double exponent_tnight;
	double exponent_tavg;
	double exponent_tsoil;

	/* nitrogen pools in gN/m2 */
	double leaf_N;
	double leaf_sun_N;
	double leaf_shade_N;
	double froot_N;
	double croot_N;
	double stem_N;
	double branch_N;
	double live_stemC_frac;                   /* fraction of live stem into stem carbon */
	double live_branchC_frac;                 /* fraction of live branch into branch carbon */
	double live_crootC_frac;                  /* fraction of live croot into coarse root carbon */

	//fixme to remove once imported into species.txt
	/* CN ratios */
	double leaf_CN;
	double croot_CN;
	double stem_CN;
	double branch_CN;
	double froot_CN;

	/* light inhibition */
	double light_inhib;                       /* (ratio) light inhibition in day-time leaf resp, see Dufrene et al., 2005 */
	double light_inhib_sun;                   /* (ratio) light inhibition for sun leaves in day-time leaf resp, see Dufrene et al., 2005 */
	double light_inhib_shade;                 /* (ratio) light inhibition for shaded leaves in day-time leaf resp, see Dufrene et al., 2005 */
	double umol_par;                          /* (umol photons/m2/sec) par converted */
	double umol_par_sun;                      /* (umol photons/m2/sec) par sun leaves converted */
	double umol_par_shade;                    /* (umol photons/m2/sec) par shade leaves converted */


	/* NOTE: No respiration happens for reserve pool (see Schwalm and Ek 2004) */

	species_t *s;
	s  = &c->heights[height].dbhs[dbh].ages[age].species[species];

	// set default value to Tbase_resp
	if (  ARE_FLOATS_EQUAL(g_settings->Tbase_resp, 0.) )
	{
		Q10_temp = 20.;
	}
	else
	{
		Q10_temp = g_settings->Tbase_resp;
	}

	logger(g_debug_log, "\n**MAINTENANCE_RESPIRATION**\n");

	/* if Prog_Aut_Resp = "on" */
	if ( g_settings->Prog_Aut_Resp )
	{
		/** maintenance respiration routine **/

		/* Uses reference values at 20 deg C (Q10_temp) and an empirical relationship between
		tissue N content and respiration rate given in:

		Ryan, M.G., 1991. Effects of climate change on plant respiration.
		Ecological Applications, 1(2):157-167.

		Uses the same value of Q_10 (2.0) for all compartments, leaf, stem,
		coarse and fine roots.

		From Ryan's figures and regressions equations, the maintenance respiration
		in kgC/day per kg of tissue N is:
		MR_ref = 0.218 (kgC/kgN/d)
		 */

		/* if acclimation for autotrophic respiration = "on" */
		//if ( g_settings->Resp_accl )
		{
			double var_a;
			double var_b;

			var_a = 3.22;
			var_b = 0.046;

			/*** NOTE: Type I acclimation (Atkin & Tjoelker 2003; Atkin et al., 2005) ***/

			/*** temperature dependent changes in Q10 function ***/

			/* based on:
			 * McGuire et al.,  (1992), Global Biogeochemical Cycles
			 * Tjoelker et al., (2001), Global Change Biology
			 * Ziehn et al.,    (2011), Geophysical Research Letters
			 * Smith and Dukes, (2013), Global Change Biology
			 * */

			/* Q10 dependent changes on temperature */
			q10_tday        = var_a - var_b * meteo_daily->tday;
			q10_tnight      = var_a - var_b * meteo_daily->tnight;
			q10_tavg        = var_a - var_b * meteo_daily->tavg;
			q10_tsoil       = var_a - var_b * meteo_daily->tsoil;

			/****************************************************************************/
		}

		/* compute exponents */
		exponent_tday   = ( meteo_daily->tday   - Q10_temp ) / 10.;
		exponent_tnight = ( meteo_daily->tnight - Q10_temp ) / 10.;
		exponent_tavg   = ( meteo_daily->tavg   - Q10_temp ) / 10.;
		exponent_tsoil  = ( meteo_daily->tsoil  - Q10_temp ) / 10.;

		/* Nitrogen content tN/cell --> gN/m2 */
#if 1
		//OLD method: constant C/N ratio of the key compartments and livewood
		// the fraction of livewood is the same for every pool (branch,stem etc)

		leaf_N          = (s->value[LEAF_N]            * 1e6 / g_settings->sizeCell);
		leaf_sun_N      = (s->value[LEAF_SUN_N]        * 1e6 / g_settings->sizeCell);
		leaf_shade_N    = (s->value[LEAF_SHADE_N]      * 1e6 / g_settings->sizeCell);
		froot_N         = (s->value[FROOT_N]           * 1e6 / g_settings->sizeCell);
		stem_N          = (s->value[STEM_LIVEWOOD_N]   * 1e6 / g_settings->sizeCell);
		croot_N         = (s->value[CROOT_LIVEWOOD_N]  * 1e6 / g_settings->sizeCell);
		branch_N        = (s->value[BRANCH_LIVEWOOD_N] * 1e6 / g_settings->sizeCell);

#else
		//TODO UNDER TEST NEW method:
		//note: differently from the old one it can (activating or NOT each single comment):
		//1 use different values for parameters
		//2 use for each live respiring C-N pools a specific-pool CN ratio
		//3 recompute live respiring pools and the related N amount

		/* from; E. Dufrene et al., Ecological Modelling 185 (2005) 407–436 */
		//todo: if accepted then move computation of live fractions into the correct source file
		//test: better if used with lower LIVE_WOOD_TURNOVER (e.g. 0.85)
#if 1
		//test: for Fagus sylvatica (from CANIF)
		leaf_CN    = 24.19;
		froot_CN   = 37.33;
		stem_CN    = 446.2;
		croot_CN   = 294.8;
		branch_CN  = 136.6;
#else
		//test: for Fagus sylvatica (from CASTANEA model)
		leaf_CN    = 20.66;
		froot_CN   = 50.50;
		stem_CN    = 416.6;
		croot_CN   = 416.6;
		branch_CN  = 90.90;
#endif

		//test: for Pinus sylvestris (to move once decided into species.txt)
		/*
		leaf_CN    = 36;
		froot_CN   = 49;
		stem_CN    = 682.00; //from 448 to 765 Hellstaen et a., 2013
		croot_CN   = 586.66; //from 445 to 761 Hellstaen et a., 2013
		branch_CN  = 454.54; //from Hyyvonen et al, 2000
		 */

		//test: for Picea abies (to move once decided into species.txt)
		/*
		leaf_CN    = 58.8;
		froot_CN   = 58.0;
		stem_CN    = 572.33; //from 448 to 765 Hellstaen et a., 2013
		croot_CN   = 400.66; //from 445 to 761 Hellstaen et a., 2013
		branch_CN  = 526.30; //from Hyyvonen et al, 2000
		 */

#if 0

		//test using new parameters (divided into stem, croot and branch) with new LIVE pools (computed using CASTANEA param on TOTAL pools)

		//fixme to move into species.txt in case used
		live_stemC_frac   = 0.21; /* for Fagus sylvatica from Dufrene et al., 2005 */
		live_branchC_frac = 0.37; /* for Fagus sylvatica from Dufrene et al., 2005 */
		live_crootC_frac  = 0.21; /* for Fagus sylvatica from Dufrene et al., 2005 */

		/*compute nitrogen fraction */
		leaf_N          = ((s->value[LEAF_C]                       / leaf_CN)   * 1e6 / g_settings->sizeCell);
		leaf_sun_N      = ((s->value[LEAF_SUN_C]                   / leaf_CN)   * 1e6 / g_settings->sizeCell);
		leaf_shade_N    = ((s->value[LEAF_SHADE_C]                 / leaf_CN)   * 1e6 / g_settings->sizeCell);
		froot_N         = ((s->value[FROOT_C]                      / froot_CN)  * 1e6 / g_settings->sizeCell);
		stem_N          = ((s->value[STEM_C]   * live_stemC_frac   / stem_CN)   * 1e6 / g_settings->sizeCell);
		croot_N         = ((s->value[CROOT_C]  * live_crootC_frac  / croot_CN)  * 1e6 / g_settings->sizeCell);
		branch_N        = ((s->value[BRANCH_C] * live_branchC_frac / branch_CN) * 1e6 / g_settings->sizeCell);

#else

		//test using new parameters (divided into stem, croot and branch) but with old LIVE pools

		/*compute nitrogen fraction */
		leaf_N          = (s->value[LEAF_C]            / leaf_CN)   * 1e6 / g_settings->sizeCell;
		leaf_sun_N      = (s->value[LEAF_SUN_C]        / leaf_CN)   * 1e6 / g_settings->sizeCell;
		leaf_shade_N    = (s->value[LEAF_SHADE_C]      / leaf_CN)   * 1e6 / g_settings->sizeCell;
		froot_N         = (s->value[FROOT_C]           / froot_CN)  * 1e6 / g_settings->sizeCell;
		stem_N          = (s->value[STEM_LIVEWOOD_C]   / stem_CN)   * 1e6 / g_settings->sizeCell;
		croot_N         = (s->value[CROOT_LIVEWOOD_C]  / croot_CN)  * 1e6 / g_settings->sizeCell;
		branch_N        = (s->value[BRANCH_LIVEWOOD_C] / branch_CN) * 1e6 / g_settings->sizeCell;

#endif
#endif


		/*******************************************************************************************************************/

		/* note: respiration values are computed in gC/m2/day at cell level*/
		/* Leaf maintenance respiration is calculated separately for day and night */

		/********************************************************************************/

		/** night-time leaf maintenance respiration **/

		/* tot leaf maintenance respiration */
		s->value[NIGHTLY_LEAF_MAINT_RESP]     = ( leaf_N       * MR_ref * pow(q10_tnight, exponent_tnight) * ( 1. - ( meteo_daily->daylength_sec / 86400. ) ) );

		/* for sun and shaded leaves */
		s->value[NIGHTLY_LEAF_SUN_MAINT_RESP]   = ( leaf_sun_N   * MR_ref * pow(q10_tnight, exponent_tnight) * ( 1. - ( meteo_daily->daylength_sec / 86400. ) ) );
		s->value[NIGHTLY_LEAF_SHADE_MAINT_RESP] = ( leaf_shade_N * MR_ref * pow(q10_tnight, exponent_tnight) * ( 1. - ( meteo_daily->daylength_sec / 86400. ) ) );

		/********************************************************************************/

		/** day-time leaf maintenance respiration **/
#if 1
		//NEW considering day-time light inhibition for leaf respiration ("Kok effect")

		//new 05/11/2017
		/* assign day-time light inhibition for leaf resp */
		/* During the day, light is assumed to inhibit this respiration according to a constant ratio 'light_inhib'.
		 * The degree of inhibition ranges between 17 and 66% depending on species
		 * (Sharp et al., 1984; Brooks and Farquhar, 1985; Kirschbaum and Farquhar, 1987, Medlyn, 1998; Wohlfart et al., 2005; Heskel et al., 2013).
		   Villar et al. (1995) give a mean rate of 51% for evergreen tree species and 62% for deciduous tree species.*/
		/* "the large discrepancy between daytime and night-time ecosystem respiration in the
		 * first half of the growing season suggests inhibition of leaf respiration by light, known as the Kok effect"
		 * but just "during the first half of the growing season only" R. Wehr, Nature 2016 */

		/* NOTE: MAESPA USES 0.6 FOR LIGHT INHIBITION */

		if ( s->value[PHENOLOGY] == 0.1 || s->value[PHENOLOGY] == 0.2 )
		{
			light_inhib       = 0.62; /* for deciduous */
			light_inhib_sun   = 0.62; /* for deciduous */
			light_inhib_shade = 0.62; /* for deciduous */
		}
		else
		{
			light_inhib       = 0.51; /* for evergreen */
			light_inhib_sun   = 0.51; /* for evergreen */
			light_inhib_shade = 0.51; /* for evergreen */
		}

		/* tot leaf maintenance respiration */
		s->value[DAILY_LEAF_MAINT_RESP]       = ( leaf_N       * MR_ref * pow(q10_tday,   exponent_tday)   * ( meteo_daily->daylength_sec / 86400. ) ) * light_inhib;

		/* for sun and shaded leaves */
		s->value[DAILY_LEAF_SUN_MAINT_RESP]   = ( leaf_sun_N   * MR_ref * pow(q10_tday,   exponent_tday)   * ( meteo_daily->daylength_sec / 86400. ) ) * light_inhib_sun;
		s->value[DAILY_LEAF_SHADE_MAINT_RESP] = ( leaf_shade_N * MR_ref * pow(q10_tday,   exponent_tday)   * ( meteo_daily->daylength_sec / 86400. ) ) * light_inhib_shade;

#else
		// old: following JULES approach (eq. 3, Mercado et al. 2007; Clark et al., 2011)

		/* molPAR/m2/day --> umolPAR/m2/sec */
		umol_par       = s->value[PAR]       * 1e6 / 86400.;
		umol_par_sun   = s->value[PAR_SUN]   * 1e6 / 86400.;
		umol_par_shade = s->value[PAR_SHADE] * 1e6 / 86400.;

		/* assuming as in Mercado et al. (2007) that below 10 umol/m2/sec no light inhibition */
		/* tot leaf maintenance respiration */
		if ( umol_par > 10. )
		{
			light_inhib = 0.5 - 0.05 * log (umol_par);
		}
		else
		{
			light_inhib = 1.;
		}
		/* for sun leaves */
		if ( umol_par_sun > 10. )
		{
			light_inhib_sun = 0.5 - 0.05 * log (umol_par_sun);
		}
		else
		{
			light_inhib_sun = 1.;
		}
		/* for shaded leaves */
		if ( umol_par_shade > 10. )
		{
			light_inhib_shade = 0.5 - 0.05 * log (umol_par_shade);
		}
		else
		{
			light_inhib_shade = 1.;
		}

		if ( meteo_daily->tday > 15 )
		{
			printf("daylength_sec = %g\n", meteo_daily->daylength_sec);
			printf("par = %g\n", s->value[PAR]);
			printf("umol_par = %g\n", umol_par);
			printf("light_inhib = %g\n", light_inhib);
			getchar();
		}

		/* tot leaf maintenance respiration */
		s->value[DAILY_LEAF_MAINT_RESP]       = ( ( s->value[NIGHTLY_LEAF_MAINT_RESP]       / ( 86400. - meteo_daily->daylength_sec ) ) * meteo_daily->daylength_sec ) * light_inhib;

		/* for sun and shaded leaves */
		s->value[DAILY_LEAF_SUN_MAINT_RESP]   = ( ( s->value[NIGHTLY_LEAF_SUN_MAINT_RESP]   / ( 86400. - meteo_daily->daylength_sec ) ) * meteo_daily->daylength_sec ) * light_inhib_sun;
		s->value[DAILY_LEAF_SHADE_MAINT_RESP] = ( ( s->value[NIGHTLY_LEAF_SHADE_MAINT_RESP] / ( 86400. - meteo_daily->daylength_sec ) ) * meteo_daily->daylength_sec ) * light_inhib_shade;
#endif

		/********************************************************************************/

		/** total leaf maintenance respiration **/

		/* total (all day) leaf maintenance respiration */
		s->value[TOT_LEAF_MAINT_RESP]         = s->value[DAILY_LEAF_MAINT_RESP] + s->value[NIGHTLY_LEAF_MAINT_RESP];

		/*******************************************************************************************************************/

		/* fine roots maintenance respiration */
		s->value[FROOT_MAINT_RESP]  = froot_N  * MR_ref * pow( q10_tsoil, exponent_tsoil );

		/*******************************************************************************************************************/

		/* live coarse root maintenance respiration */
		s->value[CROOT_MAINT_RESP]  = croot_N  * MR_ref * pow( q10_tsoil, exponent_tsoil );

		/*******************************************************************************************************************/

		/* live stem maintenance respiration */
		s->value[STEM_MAINT_RESP]   = stem_N   * MR_ref  * pow( q10_tavg, exponent_tavg );

		/*******************************************************************************************************************/

		/* live branch maintenance respiration */
		s->value[BRANCH_MAINT_RESP] = branch_N * MR_ref * pow( q10_tavg, exponent_tavg );

		/*******************************************************************************************************************/

		/* if acclimation for autotrophic respiration = "on" */
		if ( g_settings->Resp_accl )
		{
			/*** NOTE: Type II acclimation (Atkin & Tjoelker 2003; Atkin et al., 2005, Smith and Dukes, 2013) ***/

			/* Following: Atkin et al., 2008: (14), 1–18, GCB */

			/* Leaf maintenance respiration is calculated separately for day and night */
#if 0
			/* day time leaf maintenance respiration */
			s->value[DAILY_LEAF_MAINT_RESP]       *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_avg_tday   - Q10_temp ) ) );

			/* for sun and shaded leaves */
			s->value[DAILY_LEAF_SUN_MAINT_RESP]   *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_avg_tday   - Q10_temp ) ) );
			s->value[DAILY_LEAF_SHADE_MAINT_RESP] *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_avg_tday   - Q10_temp ) ) );

			/* night time leaf maintenance respiration */
			s->value[NIGHTLY_LEAF_MAINT_RESP]     *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_avg_tnight - Q10_temp ) ) );

			/* total (all day) leaf maintenance respiration */
			s->value[TOT_LEAF_MAINT_RESP]          = s->value[DAILY_LEAF_MAINT_RESP] + s->value[NIGHTLY_LEAF_MAINT_RESP];

			/*******************************************************************************************************************/

			/* fine roots maintenance respiration */
			s->value[FROOT_MAINT_RESP]            *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_avg_tsoil - Q10_temp ) ) );

			/*******************************************************************************************************************/

			/* live coarse root maintenance respiration */
			s->value[CROOT_MAINT_RESP]            *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_avg_tsoil - Q10_temp ) ) );

			/*******************************************************************************************************************/

			/* live stem maintenance respiration */
			s->value[STEM_MAINT_RESP]             *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_avg_tavg - Q10_temp ) ) );

			/*******************************************************************************************************************/

			/* live branch maintenance respiration */
			s->value[BRANCH_MAINT_RESP]           *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_avg_tavg - Q10_temp ) ) );

			/*******************************************************************************************************************/
#else
			/* day time leaf maintenance respiration */
			s->value[DAILY_LEAF_MAINT_RESP]         *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tday   - Q10_temp ) ) );

			/* day time leaf maintenance respiratio for sun and shaded leaves */
			s->value[DAILY_LEAF_SUN_MAINT_RESP]     *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tday   - Q10_temp ) ) );
			s->value[DAILY_LEAF_SHADE_MAINT_RESP]   *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tday   - Q10_temp ) ) );

			/* night time leaf maintenance respiration */
			s->value[NIGHTLY_LEAF_MAINT_RESP]       *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tnight - Q10_temp ) ) );

			/* night time leaf maintenance respiration for sun and shaded leaves */
			s->value[NIGHTLY_LEAF_SUN_MAINT_RESP]   *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tnight - Q10_temp ) ) );
			s->value[NIGHTLY_LEAF_SHADE_MAINT_RESP] *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tnight - Q10_temp ) ) );

			/* total (all day) leaf maintenance respiration */
			s->value[TOT_LEAF_MAINT_RESP]          = s->value[DAILY_LEAF_MAINT_RESP] + s->value[NIGHTLY_LEAF_MAINT_RESP];

			/*******************************************************************************************************************/

			/* fine roots maintenance respiration */
			s->value[FROOT_MAINT_RESP]            *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tsoil - Q10_temp ) ) );

			/*******************************************************************************************************************/

			/* live coarse root maintenance respiration */
			s->value[CROOT_MAINT_RESP]            *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tsoil - Q10_temp ) ) );

			/*******************************************************************************************************************/

			/* live stem maintenance respiration */
			s->value[STEM_MAINT_RESP]             *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tavg - Q10_temp ) ) );

			/*******************************************************************************************************************/

			/* live branch maintenance respiration */
			s->value[BRANCH_MAINT_RESP]           *= pow ( 10., ( acc_const * ( meteo_daily->ten_day_weighted_avg_tavg - Q10_temp ) ) );

			/*******************************************************************************************************************/

#endif
		}

		/* compute total maintenance respiration */
		s->value[TOTAL_MAINT_RESP] = (s->value[TOT_LEAF_MAINT_RESP]    +
				s->value[FROOT_MAINT_RESP]                             +
				s->value[STEM_MAINT_RESP]                              +
				s->value[CROOT_MAINT_RESP]                             +
				s->value[BRANCH_MAINT_RESP])                           ;

		/***********************************************************************************************************************/

		/* cell level */
		c->daily_leaf_maint_resp   += s->value[TOT_LEAF_MAINT_RESP];
		c->daily_stem_maint_resp   += s->value[STEM_MAINT_RESP];
		c->daily_froot_maint_resp  += s->value[FROOT_MAINT_RESP];
		c->daily_branch_maint_resp += s->value[BRANCH_MAINT_RESP];
		c->daily_croot_maint_resp  += s->value[CROOT_MAINT_RESP];
	}
	else
	{
		/** compute total maintenance respiration as a fixed value of gross productivity **/
		s->value[TOTAL_MAINT_RESP]     = s->value[GPP] * ( 1. - g_settings->Fixed_Aut_Resp_rate ) * ( 1. - GRPERC );
	}

	/* from gC/m2/day -> tC/cell/day */
	s->value[TOTAL_MAINT_RESP_tC] = s->value[TOTAL_MAINT_RESP] / 1e6 * g_settings->sizeCell;

	/* cumulate */
	s->value[MONTHLY_TOTAL_MAINT_RESP] += s->value[TOTAL_MAINT_RESP];
	s->value[YEARLY_TOTAL_MAINT_RESP]  += s->value[TOTAL_MAINT_RESP];

	/* cell level */
	c->daily_maint_resp                += s->value[TOTAL_MAINT_RESP];
	c->monthly_maint_resp              += s->value[TOTAL_MAINT_RESP];
	c->annual_maint_resp               += s->value[TOTAL_MAINT_RESP];

	/* check */
	CHECK_CONDITION(s->value[DAILY_LEAF_MAINT_RESP],   < , ZERO );
	CHECK_CONDITION(s->value[NIGHTLY_LEAF_MAINT_RESP], < , ZERO );
	CHECK_CONDITION(s->value[FROOT_MAINT_RESP],        < , ZERO );
	CHECK_CONDITION(s->value[STEM_MAINT_RESP],         < , ZERO );
	CHECK_CONDITION(s->value[CROOT_MAINT_RESP],        < , ZERO );
	CHECK_CONDITION(s->value[BRANCH_MAINT_RESP],       < , ZERO );
	CHECK_CONDITION(s->value[TOTAL_MAINT_RESP],        < , ZERO );
}

void growth_respiration(cell_t *const c, const int layer, const int height, const int dbh, const int age, const int species)
{
	species_t *s;
	s = &c->heights[height].dbhs[dbh].ages[age].species[species];

	/* NOTE: No respiration happens for reserve (see Schwalm and Ek 2004) */

	logger(g_debug_log, "\n**GROWTH_RESPIRATION**\n");

	if ( g_settings->Prog_Aut_Resp )
	{
		/* values are computed in gC/m2/day */
		if ( s->value[C_TO_LEAF]   > 0. ) s->value[LEAF_GROWTH_RESP]   = ( ( s->value[C_TO_LEAF]   * 1e6 / g_settings->sizeCell ) * s->value[EFF_GRPERC] );
		if ( s->value[C_TO_FROOT]  > 0. ) s->value[FROOT_GROWTH_RESP]  = ( ( s->value[C_TO_FROOT]  * 1e6 / g_settings->sizeCell ) * s->value[EFF_GRPERC] );
		if ( s->value[C_TO_STEM]   > 0. ) s->value[STEM_GROWTH_RESP]   = ( ( s->value[C_TO_STEM]   * 1e6 / g_settings->sizeCell ) * s->value[EFF_GRPERC] );
		if ( s->value[C_TO_CROOT]  > 0. ) s->value[CROOT_GROWTH_RESP]  = ( ( s->value[C_TO_CROOT]  * 1e6 / g_settings->sizeCell ) * s->value[EFF_GRPERC] );
		if ( s->value[C_TO_BRANCH] > 0. ) s->value[BRANCH_GROWTH_RESP] = ( ( s->value[C_TO_BRANCH] * 1e6 / g_settings->sizeCell ) * s->value[EFF_GRPERC] );

		/* compute total growth respiration */
		s->value[TOTAL_GROWTH_RESP] = (s->value[LEAF_GROWTH_RESP] +
				s->value[FROOT_GROWTH_RESP]                       +
				s->value[STEM_GROWTH_RESP]                        +
				s->value[CROOT_GROWTH_RESP]                       +
				s->value[BRANCH_GROWTH_RESP])                     ;

		/* pools */
		c->daily_leaf_growth_resp   += s->value[LEAF_GROWTH_RESP];
		c->daily_stem_growth_resp   += s->value[STEM_GROWTH_RESP];
		c->daily_froot_growth_resp  += s->value[FROOT_GROWTH_RESP];
		c->daily_branch_growth_resp += s->value[BRANCH_GROWTH_RESP];
		c->daily_croot_growth_resp  += s->value[CROOT_GROWTH_RESP];
	}
	else
	{
		/** compute total growth respiration as a fixed value of gross productivity **/
		s->value[TOTAL_GROWTH_RESP] = s->value[GPP] * ( 1. - g_settings->Fixed_Aut_Resp_rate ) * GRPERC;
	}

	/* from gC/m2/day -> tC/cell/day */
	s->value[TOTAL_GROWTH_RESP_tC] = (s->value[TOTAL_GROWTH_RESP] / 1e6 * g_settings->sizeCell);


	/* cumulate */
	s->value[MONTHLY_TOTAL_GROWTH_RESP] += s->value[TOTAL_GROWTH_RESP];
	s->value[YEARLY_TOTAL_GROWTH_RESP]  += s->value[TOTAL_GROWTH_RESP];

	/* total */
	c->daily_growth_resp                += s->value[TOTAL_GROWTH_RESP];
	c->monthly_growth_resp              += s->value[TOTAL_GROWTH_RESP];
	c->annual_growth_resp               += s->value[TOTAL_GROWTH_RESP];

	/* check */
	CHECK_CONDITION(s->value[TOTAL_GROWTH_RESP], < , ZERO );
}

void autotrophic_respiration(cell_t *const c, const int layer, const int height, const int dbh, const int age, const int species, const meteo_daily_t *const meteo_daily)
{
	species_t *s;
	s = &c->heights[height].dbhs[dbh].ages[age].species[species];

	if ( g_settings->Prog_Aut_Resp )
	{

		logger(g_debug_log, "\n**AUTOTROPHIC_RESPIRATION**\n");

		/* class level among pools */
		s->value[LEAF_AUT_RESP]            = (s->value[TOT_LEAF_MAINT_RESP]     + s->value[LEAF_GROWTH_RESP]);
		s->value[FROOT_AUT_RESP]           = (s->value[FROOT_MAINT_RESP]        + s->value[FROOT_GROWTH_RESP]);
		s->value[STEM_AUT_RESP]            = (s->value[STEM_MAINT_RESP]         + s->value[STEM_GROWTH_RESP]);
		s->value[CROOT_AUT_RESP]           = (s->value[CROOT_MAINT_RESP]        + s->value[CROOT_GROWTH_RESP]);
		s->value[BRANCH_AUT_RESP]          = (s->value[BRANCH_MAINT_RESP]       + s->value[BRANCH_GROWTH_RESP]);

		/* monthly */
		s->value[MONTHLY_LEAF_AUT_RESP]   += s->value[LEAF_AUT_RESP];
		s->value[MONTHLY_FROOT_AUT_RESP]  += s->value[FROOT_AUT_RESP];
		s->value[MONTHLY_STEM_AUT_RESP]   += s->value[STEM_AUT_RESP];
		s->value[MONTHLY_CROOT_AUT_RESP]  += s->value[CROOT_AUT_RESP];
		s->value[MONTHLY_BRANCH_AUT_RESP] += s->value[BRANCH_AUT_RESP];

		/* annual */
		s->value[YEARLY_LEAF_AUT_RESP]    += s->value[LEAF_AUT_RESP];
		s->value[YEARLY_FROOT_AUT_RESP]   += s->value[FROOT_AUT_RESP];
		s->value[YEARLY_STEM_AUT_RESP]    += s->value[STEM_AUT_RESP];
		s->value[YEARLY_CROOT_AUT_RESP]   += s->value[CROOT_AUT_RESP];
		s->value[YEARLY_BRANCH_AUT_RESP]  += s->value[BRANCH_AUT_RESP];

		/***************************************************************************************/

		/* cell level among pools */
		c->daily_leaf_aut_resp            += (s->value[TOT_LEAF_MAINT_RESP]     + s->value[LEAF_GROWTH_RESP]);
		c->daily_stem_aut_resp            += (s->value[STEM_MAINT_RESP]         + s->value[STEM_GROWTH_RESP]);
		c->daily_branch_aut_resp          += (s->value[BRANCH_MAINT_RESP]       + s->value[BRANCH_GROWTH_RESP]);
		c->daily_froot_aut_resp           += (s->value[FROOT_MAINT_RESP]        + s->value[FROOT_GROWTH_RESP]);
		c->daily_croot_aut_resp           += (s->value[CROOT_MAINT_RESP]        + s->value[CROOT_GROWTH_RESP]);
	}

	/* total */
	s->value[TOTAL_AUT_RESP]               = ( s->value[TOTAL_GROWTH_RESP] + s->value[TOTAL_MAINT_RESP] );
	s->value[TOTAL_AUT_RESP_tC]            = ( s->value[TOTAL_AUT_RESP] / 1e6 * g_settings->sizeCell );
	s->value[MONTHLY_TOTAL_AUT_RESP]      += s->value[TOTAL_AUT_RESP];
	s->value[YEARLY_TOTAL_AUT_RESP]       += s->value[TOTAL_AUT_RESP];

	logger(g_debug_log, "daily total autotrophic respiration (%s) = %g gC/m2/day\n",   s->name, s->value[TOTAL_AUT_RESP]);
	logger(g_debug_log, "daily total autotrophic respiration (%s) = %g tC/cell/day\n", s->name, s->value[TOTAL_AUT_RESP_tC]);

	/* cell level */
	c->daily_aut_resp                     += s->value[TOTAL_AUT_RESP];
	c->monthly_aut_resp                   += s->value[TOTAL_AUT_RESP];
	c->annual_aut_resp                    += s->value[TOTAL_AUT_RESP];
	c->daily_aut_resp_tC                  += (s->value[TOTAL_AUT_RESP] / 1e6 * g_settings->sizeCell);
	c->monthly_aut_resp_tC                += (s->value[TOTAL_AUT_RESP] / 1e6 * g_settings->sizeCell);
	c->annual_aut_resp_tC                 += (s->value[TOTAL_AUT_RESP] / 1e6 * g_settings->sizeCell);

	/* check */
	CHECK_CONDITION( s->value[TOTAL_AUT_RESP], < , ZERO );
}

void growth_respiration_frac ( const age_t *const a, species_t *const s )
{
	int min_age;               /* minimum age for max growth respiration fraction */
	int max_age;               /* maximum age for min growth respiration fraction */

	min_age = 1;
	max_age = (int)s->value[MAXAGE];

	/* age-dependant growth respiration fraction */
	/* see Waring and Running 1998, "Forest Ecosystem - Analysis at Multiple Scales"
	 * see Ryan 1991, Ecological Applications
	 * see Ryan 1991, Tree Physiology */

	s->value[EFF_GRPERC] = ( GRPERCMIN - GRPERCMAX ) / ( max_age - min_age ) * ( a->value - min_age ) + GRPERCMAX;
	if( a->value > s->value[MAXAGE]) s->value[EFF_GRPERC] = GRPERCMIN;
}




