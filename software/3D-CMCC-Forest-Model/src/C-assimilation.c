/*
 * C-assimilation.c
 *
 *  Created on: 14/ott/2013
 *      Author: alessio
 */
/* includes */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "C-assimilation.h"
#include "constants.h"
#include "settings.h"
#include "logger.h"

extern settings_t* g_settings;
extern logger_t* g_debug_log;


void carbon_productivity(cell_t *const c, const int height, const int dbh, const int age, const int species)
{
	species_t *s;
	s = &c->heights[height].dbhs[dbh].ages[age].species[species];

	logger (g_debug_log, "\n**C-ASSIMILATION**\n");

	/* NPP computation is based on ground surface area */
	s->value[NPP]    = s->value[GPP] - s->value[TOTAL_AUT_RESP];
	s->value[NPP_tC] = s->value[NPP] / 1e6 * g_settings->sizeCell;

	/* class level */
	s->value[MONTHLY_NPP]    += s->value[NPP];
	s->value[YEARLY_NPP]     += s->value[NPP];
	s->value[MONTHLY_NPP_tC] += s->value[NPP_tC];
	s->value[YEARLY_NPP_tC]  += s->value[NPP_tC];

	s->value[MONTHLY_BP]     += s->value[BP];
	s->value[YEARLY_BP]      += s->value[BP];

	/* cell level */
	c->daily_npp             += s->value[NPP];
	c->monthly_npp           += s->value[NPP];
	c->annual_npp            += s->value[NPP];
	c->daily_npp_tC          += s->value[NPP_tC];
	c->monthly_npp_tC        += s->value[NPP_tC];
	c->annual_npp_tC         += s->value[NPP_tC];

}



