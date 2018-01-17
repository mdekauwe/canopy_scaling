/* ============================================================================
* Just run MATE/2-leaf model to look at scaling effect on CO2 response of
* photosynthesis
*
* AUTHOR:
*   Martin De Kauwe
*
* DATE:
*   17.01.2018
*
* =========================================================================== */

#include "canopy_scaling.h"

int main(int argc, char **argv)
{
    int error = 0;

    /*
     * Setup structures, initialise stuff, e.g. zero fluxes.
     */
    control *c;
    canopy_wk *cw;
    fluxes *f;
    met_arrays *ma;
    met *m;
    params *p;
    state *s;

    c = (control *)malloc(sizeof(control));
    if (c == NULL) {
        fprintf(stderr, "control structure: Not allocated enough memory!\n");
    	exit(EXIT_FAILURE);
    }

    cw = (canopy_wk *)malloc(sizeof(canopy_wk));
    if (cw == NULL) {
        fprintf(stderr, "canopy wk structure: Not allocated enough memory!\n");
    	exit(EXIT_FAILURE);
    }

    f = (fluxes *)malloc(sizeof(fluxes));
    if (f == NULL) {
    	fprintf(stderr, "fluxes structure: Not allocated enough memory!\n");
    	exit(EXIT_FAILURE);
    }

    ma = (met_arrays *)malloc(sizeof(met_arrays));
    if (ma == NULL) {
    	fprintf(stderr, "met arrays structure: Not allocated enough memory!\n");
    	exit(EXIT_FAILURE);
    }

    m = (met *)malloc(sizeof(met));
    if (m == NULL) {
    	fprintf(stderr, "met structure: Not allocated enough memory!\n");
    	exit(EXIT_FAILURE);
    }

    p = (params *)malloc(sizeof(params));
    if (p == NULL) {
    	fprintf(stderr, "params structure: Not allocated enough memory!\n");
    	exit(EXIT_FAILURE);
    }

    s = (state *)malloc(sizeof(state));
    if (s == NULL) {
    	fprintf(stderr, "state structure: Not allocated enough memory!\n");
    	exit(EXIT_FAILURE);
    }

    // potentially allocating 1 extra spot, but will be fine as we always
    // index by num_days
    if ((s->day_length = (double *)calloc(366, sizeof(double))) == NULL) {
        fprintf(stderr,"Error allocating space for day_length\n");
		exit(EXIT_FAILURE);
    }

    initialise_control(c);
    initialise_params(p);
    initialise_fluxes(f);
    initialise_state(s);

    clparser(argc, argv, c);
    /*
     * Read .ini parameter file and meterological data
     */
    error = parse_ini_file(c, p, s);
    if (error != 0) {
        prog_error("Error reading .INI file on line", __LINE__);
    }
    strcpy(c->git_code_ver, build_git_sha);
    if (c->PRINT_GIT) {
        fprintf(stderr, "\n%s\n", c->git_code_ver);
        exit(EXIT_FAILURE);
    }

    if (c->sub_daily) {
        read_subdaily_met_data(argv, c, ma);
        fill_up_solar_arrays(cw, c, ma, p);
    } else {
        read_daily_met_data(argv, c, ma);
    }

    s->wtfac_topsoil = 1.0;
    s->wtfac_root = 1.0;

    run_sim(cw, c, f, ma, m, p, s);

    /* clean up */
    fclose(c->ofp);
    if (c->print_options == SUBDAILY ) {
        fclose(c->ofp_sd);
    }
    fclose(c->ifp);
    if (c->output_ascii == FALSE) {
        fclose(c->ofp_hdr);
    }

    free(cw);
    free(c);
    free(ma->year);
    free(ma->tair);
    free(ma->rain);
    free(ma->tsoil);
    free(ma->co2);
    free(ma->ndep);
    free(ma->wind);
    free(ma->press);
    free(ma->par);
    if (c->sub_daily) {
        free(ma->vpd);
        free(ma->doy);
        free(cw->cz_store);
        free(cw->ele_store);
        free(cw->df_store);
    } else {
        free(ma->prjday);
        free(ma->tam);
        free(ma->tpm);
        free(ma->tmin);
        free(ma->tmax);
        free(ma->vpd_am);
        free(ma->vpd_pm);
        free(ma->wind_am);
        free(ma->wind_pm);
        free(ma->par_am);
        free(ma->par_pm);
    }
    free(s->day_length);
    free(ma);
    free(m);
    free(p);
    free(s);
    free(f);

    exit(EXIT_SUCCESS);
}

void run_sim(canopy_wk *cw, control *c, fluxes *f, met_arrays *ma, met *m,
             params *p, state *s) {

    int    nyr, doy, window_size, i, dummy = 0;
    int    fire_found = FALSE;;
    int    num_disturbance_yrs = 0;

    double nitfac, year;
    double leafn, fc, ncontent;
    /*
     * Params are defined in per year, needs to be per day. Important this is
     * done here as rate constants elsewhere in the code are assumed to be in
     * units of days not years
     */
    correct_rate_constants(p, FALSE);

    s->lai = p->fix_lai;

    /* ====================== **
    **   Y E A R    L O O P   **
    ** ====================== */
    c->day_idx = 0;
    c->hour_idx = 0;

    printf("year,doy,gpp\n");

    for (nyr = 0; nyr < c->num_years; nyr++) {

        if (c->sub_daily) {
            year = ma->year[c->hour_idx];
        } else {
            year = ma->year[c->day_idx];
        }

        if (is_leap_year(year))
            c->num_days = 366;
        else
            c->num_days = 365;

        calculate_daylength(s, c->num_days, p->latitude);

        /* =================== **
        **   D A Y   L O O P   **
        ** =================== */
        for (doy = 0; doy < c->num_days; doy++) {

            if (! c->sub_daily) {
                unpack_met_data(c, f, ma, m, dummy, s->day_length[doy]);
            }

            if (c->sub_daily) {
                canopy(cw, c, f, ma, m, p, s);
            } else {
                if (s->lai > 0.0) {
                    /* average leaf nitrogen content (g N m-2 leaf) */
                    leafn = (s->shootnc * p->cfracts / p->sla * KG_AS_G);

                    /* total nitrogen content of the canopy */
                    ncontent = leafn * s->lai;
                } else {
                    ncontent = 0.0;
                }

                /*
                    When canopy is not closed, canopy light interception is
                    reduced - calculate the fractional ground cover
                */
                if (s->lai < p->lai_closed) {
                    /* discontinuous canopies */
                    fc = s->lai / p->lai_closed;
                } else {
                    fc = 1.0;
                }

                /*
                    fIPAR - the fraction of intercepted PAR = IPAR/PAR incident
                    at the top of the canopy, accounting for partial closure based on Jackson and Palmer (1979).
                */
                if (s->lai > 0.0)
                    s->fipar = ((1.0 - exp(-p->kext * s->lai / fc)) * fc);
                else
                    s->fipar = 0.0;

                mate_C3_photosynthesis(c, f, m, p, s, s->day_length[doy],
                                       ncontent);
                printf("%d,%d,%lf\n", (int)year, doy, f->gpp*100.);
            }

            c->day_idx++;
            /* ======================= **
            **   E N D   O F   D A Y   **
            ** ======================= */
        }

    }
    /* ========================= **
    **   E N D   O F   Y E A R   **
    ** ========================= */
    correct_rate_constants(p, TRUE);

    return;


}

void clparser(int argc, char **argv, control *c) {
    int i;

    for (i = 1; i < argc; i++) {
        if (*argv[i] == '-') {
            if (!strncasecmp(argv[i], "-p", 2)) {
			    strcpy(c->cfg_fname, argv[++i]);
            } else if (!strncasecmp(argv[i], "-s", 2)) {
                c->spin_up = TRUE;
            } else if (!strncasecmp(argv[i], "-ver", 4)) {
                c->PRINT_GIT = TRUE;
            } else if (!strncasecmp(argv[i], "-u", 2) ||
                       !strncasecmp(argv[i], "-h", 2)) {
                usage(argv);
                exit(EXIT_FAILURE);
            } else {
                fprintf(stderr, "%s: unknown argument on command line: %s\n",
                               argv[0], argv[i]);
                usage(argv);
                exit(EXIT_FAILURE);
            }
        }
    }
    return;
}


void usage(char **argv) {
    fprintf(stderr, "\n========\n");
    fprintf(stderr, " USAGE:\n");
    fprintf(stderr, "========\n");
    fprintf(stderr, "%s [options]\n", argv[0]);
    fprintf(stderr, "\n\nExpected input file is a .ini/.cfg style param file, passed with the -p flag .\n");
    fprintf(stderr, "\nThe options are:\n");
    fprintf(stderr, "\n++General options:\n" );
    fprintf(stderr, "[-ver          \t] Print the git hash tag.]\n");
    fprintf(stderr, "[-p       fname\t] Location of parameter file (.ini/.cfg).]\n");
    fprintf(stderr, "[-s            \t] Spin-up GDAY, when it the model is finished it will print the final state to the param file.]\n");
    fprintf(stderr, "\n++Print this message:\n" );
    fprintf(stderr, "[-u/-h         \t] usage/help]\n");

    return;
}

void correct_rate_constants(params *p, int output) {
    /* adjust rate constants for the number of days in years */

    if (output) {
        p->rateuptake *= NDAYS_IN_YR;
        p->rateloss *= NDAYS_IN_YR;
        p->retransmob *= NDAYS_IN_YR;
        p->fdecay *= NDAYS_IN_YR;
        p->fdecaydry *= NDAYS_IN_YR;
        p->crdecay *= NDAYS_IN_YR;
        p->rdecay *= NDAYS_IN_YR;
        p->rdecaydry *= NDAYS_IN_YR;
        p->bdecay *= NDAYS_IN_YR;
        p->wdecay *= NDAYS_IN_YR;
        p->sapturnover *= NDAYS_IN_YR;
        p->kdec1 *= NDAYS_IN_YR;
        p->kdec2 *= NDAYS_IN_YR;
        p->kdec3 *= NDAYS_IN_YR;
        p->kdec4 *= NDAYS_IN_YR;
        p->kdec5 *= NDAYS_IN_YR;
        p->kdec6 *= NDAYS_IN_YR;
        p->kdec7 *= NDAYS_IN_YR;
        p->nuptakez *= NDAYS_IN_YR;
        p->nmax *= NDAYS_IN_YR;
    } else {
        p->rateuptake /= NDAYS_IN_YR;
        p->rateloss /= NDAYS_IN_YR;
        p->retransmob /= NDAYS_IN_YR;
        p->fdecay /= NDAYS_IN_YR;
        p->fdecaydry /= NDAYS_IN_YR;
        p->crdecay /= NDAYS_IN_YR;
        p->rdecay /= NDAYS_IN_YR;
        p->rdecaydry /= NDAYS_IN_YR;
        p->bdecay /= NDAYS_IN_YR;
        p->wdecay /= NDAYS_IN_YR;
        p->sapturnover /= NDAYS_IN_YR;
        p->kdec1 /= NDAYS_IN_YR;
        p->kdec2 /= NDAYS_IN_YR;
        p->kdec3 /= NDAYS_IN_YR;
        p->kdec4 /= NDAYS_IN_YR;
        p->kdec5 /= NDAYS_IN_YR;
        p->kdec6 /= NDAYS_IN_YR;
        p->kdec7 /= NDAYS_IN_YR;
        p->nuptakez /= NDAYS_IN_YR;
        p->nmax /= NDAYS_IN_YR;
    }

    return;
}

void unpack_met_data(control *c, fluxes *f, met_arrays *ma, met *m, int hod,
                     double day_length) {

    double c1, c2;

    /* unpack met forcing */
    if (c->sub_daily) {
        m->rain = ma->rain[c->hour_idx];
        m->wind = ma->wind[c->hour_idx];
        m->press = ma->press[c->hour_idx] * KPA_2_PA;
        m->vpd = ma->vpd[c->hour_idx] * KPA_2_PA;
        m->tair = ma->tair[c->hour_idx];
        m->tsoil = ma->tsoil[c->hour_idx];
        m->par = ma->par[c->hour_idx];
        m->sw_rad = ma->par[c->hour_idx] * PAR_2_SW; /* W m-2 */
        m->Ca = ma->co2[c->hour_idx];

        /* NDEP is per 30 min so need to sum 30 min data */
        if (hod == 0) {
            m->ndep = ma->ndep[c->hour_idx];
            m->nfix = ma->nfix[c->hour_idx];
        } else {
            m->ndep += ma->ndep[c->hour_idx];
            m->nfix += ma->nfix[c->hour_idx];
        }
    } else {
        m->Ca = ma->co2[c->day_idx];
        m->tair = ma->tair[c->day_idx];
        m->tair_am = ma->tam[c->day_idx];
        m->tair_pm = ma->tpm[c->day_idx];
        m->par = ma->par_am[c->day_idx] + ma->par_pm[c->day_idx];

        /* Conversion factor for PAR to SW rad */
        c1 = MJ_TO_J * J_2_UMOL / (day_length * 60.0 * 60.0) * PAR_2_SW;
        c2 = MJ_TO_J * J_2_UMOL / (day_length / 2.0 * 60.0 * 60.0) * PAR_2_SW;
        m->sw_rad = m->par * c1;
        m->sw_rad_am = ma->par_am[c->day_idx] * c2;
        m->sw_rad_pm = ma->par_pm[c->day_idx] * c2;
        m->rain = ma->rain[c->day_idx];
        m->vpd_am = ma->vpd_am[c->day_idx] * KPA_2_PA;
        m->vpd_pm = ma->vpd_pm[c->day_idx] * KPA_2_PA;
        m->wind_am = ma->wind_am[c->day_idx];
        m->wind_pm = ma->wind_pm[c->day_idx];
        m->press = ma->press[c->day_idx] * KPA_2_PA;
        m->ndep = ma->ndep[c->day_idx];
        m->nfix = ma->nfix[c->day_idx];
        m->tsoil = ma->tsoil[c->day_idx];
        m->Tk_am = ma->tam[c->day_idx] + DEG_TO_KELVIN;
        m->Tk_pm = ma->tpm[c->day_idx] + DEG_TO_KELVIN;

        /*printf("%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n",
               m->Ca, m->tair, m->tair_am, m->tair_pm, m->par, m->sw_rad,
               m->sw_rad_am, m->sw_rad_pm, m->rain, m->vpd_am, m->vpd_pm,
               m->wind_am, m->wind_pm, m->press, m->ndep, m->tsoil, m->Tk_am,
               m->Tk_pm);*/

    }

    /* N deposition + biological N fixation */
    f->ninflow = m->ndep + m->nfix;

    return;
}


void fill_up_solar_arrays(canopy_wk *cw, control *c, met_arrays *ma, params *p) {

    // This is a suprisingly big time hog. So I'm going to unpack it once into
    // an array which we can then access during spinup to save processing time

    int    nyr, doy, hod;
    long   ntimesteps = c->total_num_days * 48;
    double year, sw_rad;

    cw->cz_store = malloc(ntimesteps * sizeof(double));
    if (cw->cz_store == NULL) {
        fprintf(stderr, "malloc failed allocating cz store\n");
        exit(EXIT_FAILURE);
    }

    cw->ele_store = malloc(ntimesteps * sizeof(double));
    if (cw->ele_store == NULL) {
        fprintf(stderr, "malloc failed allocating ele store\n");
        exit(EXIT_FAILURE);
    }

    cw->df_store = malloc(ntimesteps * sizeof(double));
    if (cw->df_store == NULL) {
        fprintf(stderr, "malloc failed allocating df store\n");
        exit(EXIT_FAILURE);
    }

    c->hour_idx = 0;
    for (nyr = 0; nyr < c->num_years; nyr++) {
        year = ma->year[c->hour_idx];
        if (is_leap_year(year))
            c->num_days = 366;
        else
            c->num_days = 365;
        for (doy = 0; doy < c->num_days; doy++) {
            for (hod = 0; hod < c->num_hlf_hrs; hod++) {
                calculate_solar_geometry(cw, p, doy, hod);
                sw_rad = ma->par[c->hour_idx] * PAR_2_SW; /* W m-2 */
                get_diffuse_frac(cw, doy, sw_rad);
                cw->cz_store[c->hour_idx] = cw->cos_zenith;
                cw->ele_store[c->hour_idx] = cw->elevation;
                cw->df_store[c->hour_idx] = cw->diffuse_frac;
                c->hour_idx++;
            }
        }
    }
    return;

}
