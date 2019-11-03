#pragma once
#pragma once
typedef struct PMS7003
{
	short concPM1_0;
	short conc_PM2_5;
	short conc_PM10_0;
	short raw0_3;
	short raw0_5;
	short raw1_0;
	short raw2_5;
	short raw5_0;
	short raw10_0;
	short error;
};

struct PMS7003 get_pms7003(int);