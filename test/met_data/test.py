import pandas as pd
import calendar

fname = "xxx"
df = pd.read_csv(fname)

i = 0
for yr in range(2014, 2014 + 1):

    if calendar.isleap(yr):
        ndays = 366
    else:
        ndays = 365

    for d in range(1, ndays + 1):

        co2 = 0.0
        for h in range(48):

            co2 += df.co2[i]
            i += 1
        print((co2 / 48.) / 390.)
