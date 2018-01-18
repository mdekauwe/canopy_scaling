#!/usr/bin/env python

"""
Ozflux simulations: Tumbarumbas

(i) Spin-up using 20 years of recycled met data: CO2 = 285; n-cycle off,
    LAI precribed from MODIS.
(ii) Run simulation using met forcing years, with CO2 varying.

For these simulations we are going to paramaterise GDAY as best we can using
CABLE's evergreen broadleaf forest paramaters.
"""

import numpy as np
import os
import shutil
import sys
import subprocess

__author__  = "Martin De Kauwe"
__version__ = "1.0 (31.03.2015)"
__email__   = "mdekauwe@gmail.com"

USER = os.getlogin()
import adjust_gday_param_file as ad

def main(experiment_id, latitude, longitude, treatment, scheme):

    GDAY = "canopy_scaling -p "

    # dir names
    base_param_name = "base_start"
    base_dir = os.path.dirname(os.getcwd())
    base_param_dir = "/Users/%s/Desktop/canopy_scaling/params" % (USER)

    param_dir = os.path.join(base_dir, "params")
    met_dir = os.path.join(base_dir, "met_data")
    run_dir = os.path.join(base_dir, "outputs")

    # copy base files to make two new experiment files
    shutil.copy(os.path.join(base_param_dir, base_param_name + ".cfg"),
                os.path.join(param_dir, "%s_model_run.cfg" % \
                                                (experiment_id)))

    if scheme == "mate":
        sub_daily = "false"
        alpha_j = 0.26
    else:
        sub_daily = "true"
        alpha_j = 0.308

    itag = "%s_model_run" % (experiment_id)
    otag = "%s_simulation" % (experiment_id)
    mtag = "%s_%s.csv" % (scheme, treatment)

    out_fn = "%s_simulation.csv" % (experiment_id)
    out_param_fname = os.path.join(param_dir, otag + ".cfg")
    cfg_fname = os.path.join(param_dir, itag + ".cfg")
    met_fname = os.path.join(met_dir, mtag)

    out_fname = os.path.join(run_dir, out_fn)
    replace_dict = {

                     # files
                     "out_param_fname": "%s" % (out_param_fname),
                     "cfg_fname": "%s" % (cfg_fname),
                     "met_fname": "%s" % (met_fname),
                     "out_fname": "%s" % (out_fname),

                     "fix_lai": "1.2",
                     #"alpha_j": "0.30588",
                     "alpha_j": "%f" % (alpha_j),

                     "max_intercep_lai": "3.0",
                     "latitude": "%f" % (latitude),
                     "longitude": "%f" % (longitude),

                     "slamax": "4.37",    # 43.7 +/- 1.5 cm2 g 1 dry mass
                     "sla": "4.37",       # 43.7 +/-  1.5 cm2 g 1 dry mass
                     "slazero": "4.37",   # 43.7+/-  1.5 cm2 g 1 dry mass
                     "lai_cover": "0.5",
                     "kn": "0.3",
                     "g1": "3.8667",      # Fit by Me to Teresa's data 7th Nov 2013
                     "jmax": "55.0",   # 2.0 * vcmax
                     "vcmax": "110.0",   # CABLE EBF
                     "jmaxna": "110.0",
                     "jmaxnb": "0.0",
                     "vcmaxna": "55.0",
                     "vcmaxnb": "0.0",
                     "measurement_temp": "25.0",


                     "prescribed_leaf_NC": "0.03",

                     # control

                     "assim_model": "mate",
                     "fixed_lai": "true",
                     "gs_model": "medlyn",
                     "modeljm": "1",
                     "ncycle": "false",
                     "ps_pathway": "c3",
                     "respiration_model": "fixed",
                     "sub_daily": "%s" % (sub_daily),
                     "sw_stress_model": "1",  # Sands and Landsberg
                     "water_stress": "false",

                    }
    ad.adjust_param_file(cfg_fname, replace_dict)
    ofname = " > outputs/%s_%s.csv" % (scheme, treatment)
    #print(cfg_fname)
    os.system(GDAY + cfg_fname + ofname)
    #os.system(GDAY + cfg_fname)



if __name__ == "__main__":

    experiment_id = "EucFACE"

    latitude = -35.6566
    longitude = 148.152

    main(experiment_id, latitude, longitude, treatment="AMB", scheme="twoleaf")
    main(experiment_id, latitude, longitude, treatment="ELE", scheme="twoleaf")

    main(experiment_id, latitude, longitude, treatment="AMB", scheme="mate")
    main(experiment_id, latitude, longitude, treatment="ELE", scheme="mate")
