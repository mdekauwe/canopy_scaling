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

                days_data = df[(df.year == yr) & (df.doy == d)]

                morning = days_data[(days_data.hod < 11) &
                                    (days_data.hod < 24)]
                afternoon = days_data[(days_data.hod >= 24) &
                                      (days_data.hod < 37)]

                tair = np.mean(np.hstack((morning["tair"], \
                                           afternoon["tair"])))

                tam = np.mean(morning["tair"])
                tpm = np.mean(afternoon["tair"])

                tsoil = np.mean(days_data["tair"])

                # daytime min/max temp
                tmin = np.min(days_data["tair"])
                tmax = np.max(days_data["tair"])
                tday = np.mean(days_data["tair"])

                if len(morning) == 0:
                    vpd_am = 0.05
                else:
                    vpd_am = np.mean(morning.vpd)
                    if vpd_am < 0.05:
                        vpd_am = 0.05

                if len(afternoon) == 0:
                    vpd_pm = 0.05
                else:
                    vpd_pm = np.mean(afternoon.vpd)
                    if vpd_pm < 0.05:
                        vpd_pm = 0.05


                # convert PAR [umol m-2 s-1] -> mj m-2 30min-1
                conv = UMOL_TO_J * J_TO_MJ * SEC_TO_HFHR
                par_am = np.sum(morning.par * conv)
                par_pm = np.sum(afternoon.par * conv)

                # rain -> mm
                # coversion 1800 seconds to half hours and summed gives day value.
                # Going to use the whole day including the night data
                rain = np.sum(days_data.rain)

                # wind speed -> m/s
                wind = np.mean(np.hstack((morning.wind, afternoon.wind)))

                # odd occasions whne there is no data, so set a very small wind speed.
                wind_am = np.mean(morning.wind)
                if len(morning.wind) == 0:
                    wind_am = 0.1
                wind_pm = np.mean(afternoon.wind)
                if len(afternoon.wind) == 0:
                    wind_pm = 0.1

                # air pressure -> kPa
                press = 101.32


                if co2_conc is not None:
                    co2 = co2_conc
                else:
                    #co2 = np.mean(np.hstack((morning["co2"], \
                    #                         afternoon["co2"])))
                    #co22 = np.median(np.hstack((morning["co2"], \
                    #                         afternoon["co2"])))

                    co2 = 390 * 1.3

                wr.writerow([yr, d, tair, rain, tsoil, tam, tpm, tmin, tmax,\
                             tday, vpd_am, vpd_pm, co2, -999.9, -999.9, wind, \
                             press, wind_am, wind_pm, par_am, par_pm])

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
