#!/usr/bin/env python2.7

"""
Create a G'DAY met forcing file from the MAESPA-style met file

That's all folks.
"""
__author__ = "Martin De Kauwe"
__version__ = "1.0 (17.01.2017)"
__email__ = "mdekauwe@gmail.com"

import sys
import os
import csv
import math
import numpy as np
from datetime import date
import calendar
import pandas as pd
try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO
import datetime as dt
import calendar


class CreateMetData(object):

    def __init__(self, site, fname):

        self.fname = fname
        self.site = site
        self.ovar_names = ['#year', 'doy', 'tair', 'rain', 'tsoil',
                           'tam', 'tpm', 'tmin', 'tmax', 'tday', 'vpd_am',
                           'vpd_pm', 'co2', 'ndep', 'nfix', 'wind', 'pres',
                           'wind_am', 'wind_pm', 'par_am', 'par_pm']
        self.ounits = ['#--', '--', 'degC', 'mm/d', 'degC','degC', 'degC',
                       'degC','degC', 'degC', 'kPa', 'kPa', 'ppm', 't/ha/d',
                       't/ha/d', 'm/s', 'kPa', 'm/s', 'm/s', 'mj/m2/d',
                       'mj/m2/d']
        self.lat = -999.9
        self.lon = -999.9

        # unit conversions
        self.SEC_TO_HFHR = 60.0 * 30.0 # sec/half hr
        self.PA_TO_KPA = 0.001
        self.J_TO_MJ = 1.0E-6
        self.K_to_C = 273.15
        self.G_M2_to_TONNES_HA = 0.01
        self.MM_S_TO_MM_HR = 3600.0
        self.SW_2_PAR = 2.3


    def main(self, ofname, scheme, co2_conc=None):

        SEC_TO_HFHR = 60.0 * 30.0 # sec/half hr
        J_TO_UMOL = 4.57
        UMOL_TO_J = 1.0 / J_TO_UMOL
        J_TO_MJ = 1.0E-6

        df = pd.read_csv(self.fname, sep=" ")

        yr_sequence = np.unique(df.year)
        start_sim = yr_sequence[0]
        end_sim = yr_sequence[-1]

        try:
            ofp = open(ofname, 'wb')
            wr = csv.writer(ofp, delimiter=',', quoting=csv.QUOTE_NONE,
                            escapechar=None, dialect='excel')
            wr.writerow(['# %s 30 min met forcing' % (self.site)])
            wr.writerow(['# Data from %s-%s' % (start_sim, end_sim)])
            wr.writerow(['# Created by Martin De Kauwe: %s' % date.today()])
            wr.writerow([var for i, var in enumerate(self.ounits)])
            wr.writerow([var for i, var in enumerate(self.ovar_names)])

        except IOError:
            raise IOError('Could not write met file: "%s"' % ofname)


        for yr in range(start_sim, end_sim + 1):

            if calendar.isleap(yr):
                ndays = 366
            else:
                ndays = 365

            for d in range(1, ndays + 1):


                rain = 1.0
                par = 1800.0
                tair = 20.0
                tam = 20.0
                tpm = 20.0
                tmin = 5.0
                tmax = 20.0
                tday = 20.0
                tsoil = 10.0
                vpd_am = 2.0
                vpd_pm = 2.0
                conv = UMOL_TO_J * J_TO_MJ * SEC_TO_HFHR * 12 * 2.
                par_am = par * conv
                par_pm = par * conv

                if co2_conc is not None:
                    co2 = co2_conc
                else:
                    co2 = 390 + 150.
                ndep = -999.9
                nfix = -999.9
                wind_am = 1.5
                wind_pm = 1.5
                wind = 1.5
                press = 101.325


                wr.writerow([float(yr), float(d),tair, rain,tsoil,\
                             tam,tpm,tmin,tmax,tday,
                             vpd_am,vpd_pm,co2,ndep,nfix,
                             wind,press,wind_am,wind_pm,
                             par_am,par_pm])

        ofp.close()


if __name__ == "__main__":

    fdir = "raw_data"
    fname = os.path.join(fdir, "gdayMet.dat")
    site = "EucFACE"

    scheme = "mate"

    ofname = "%s_AMB.csv" % (scheme)
    C = CreateMetData(site, fname)
    C.main(ofname, scheme, co2_conc=390.0)

    ofname = "%s_ELE.csv" % (scheme)
    C.main(ofname, scheme, co2_conc=None)
