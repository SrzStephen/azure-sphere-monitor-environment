#define UART_STRUCTS_VERSION 1
#include <applibs/uart.h>
#include <applibs/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "mh_z19.h"
#include <stdio.h> 
#include <string.h> 
#define MAX_READ_SECONDS 10
struct MHZ19_DATA mhz19_data;
unsigned char gas_reading_array[9] = { 0xff,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79 };
int bytes_written = -1;
int bytes_read = 0;
unsigned char data_buffer[9];
long start_time;
long end_time;
char check;
bool valid_checksum(unsigned char data_array[])
{
	check = 0;
	for (char ii = 0; ii < 8; ii++)
	{
		check += data_array[ii];
	}
	return (0xff - check) == data_array[8];

}

struct MHZ19_DATA get_mh_z19_value(int fd)
{
	//int fd = fd_pointer;
	start_time = time(NULL);
	end_time = time(NULL);
	bytes_written = -1;
	bytes_read = 0;
	bytes_written = write(fd, gas_reading_array, 9);
	memset(data_buffer, 0, sizeof(data_buffer));
	if (bytes_written != 9)
	{
		//return -2;
	}
	// used to read the 9 byte response

	// check that we read 9 bytes, 
	while (bytes_read != 9 && (end_time - start_time) < MAX_READ_SECONDS)
	{
		end_time = time(NULL);
		bytes_read = read(fd, data_buffer, 9);

		if (bytes_read > 0)
		{
			// check checksum is valid & check that we've read the header properly.
			if (valid_checksum(data_buffer) && data_buffer[0] == 0xff)
			{
				mhz19_data.co2ppm = data_buffer[3] + data_buffer[2] * 256;
				mhz19_data.temperature = data_buffer[4] - 40;
				mhz19_data.error = 1;
				return mhz19_data;
			}
		}


	}
	mhz19_data.error = -3;
	return mhz19_data;
}

bool calibrate_mh_z19_zero(int fd)
{
	int zero_calibrate[8] = { 255,1,135,0,0,0,0,0,120 };
	//return write_bytes(zero_calibrate,fd);
	return false;

}
bool calibrate_mhz_z19_span(int fd, int lower, int upper)
{
	int calibrate_span[8] = { 255,1,136,upper,lower,0,0,0,120 };
	//return write_bytes(calibrate_span, fd);
	return false;
}