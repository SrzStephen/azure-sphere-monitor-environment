#pragma once
typedef struct MHZ19_DATA
{
	int co2ppm;
	int temperature;
	int error;
};


// calculate zero point
bool calibrate_mh_z19_zero(int fd);
// calibrate span
bool calibrate_mhz_z19_span(int fd, int lower, int upper); \
// get value

struct MHZ19_DATA get_mh_z19_value(int fd);

#pragma once
