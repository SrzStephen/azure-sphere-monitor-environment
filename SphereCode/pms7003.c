#define UART_STRUCTS_VERSION 1
#include "pms7003.h"
#include <applibs/uart.h>
#include <applibs/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h> 
#include <string.h> 
#define MAX_READ_SECONDS 10
struct PMS7003 data_7003;
struct PMS7003 pms7003;
unsigned char activate_command[7] = { 0x42, 0x4D, 0xE1, 0x00, 0x01, 0x01, 0x71 };
long start_time = 0;
long end_time = 0;
uint8_t initial_buffer[64];
uint8_t data_buffer[32];
size_t bytes_read;
bool header_found = false;
bool checksum_bool = false;

struct PMS7003 parse_pms7003(uint8_t byte_array[])
{

	pms7003.concPM1_0 = byte_array[9] + (byte_array[8] << 8);
	pms7003.conc_PM2_5 = byte_array[11] + (byte_array[10] << 8);
	pms7003.conc_PM10_0 = byte_array[13] + (byte_array[12] << 8);
	pms7003.raw0_3 = byte_array[15] + (byte_array[14] << 8);
	pms7003.raw0_5 = byte_array[17] + (byte_array[16] << 8);
	pms7003.raw1_0 = byte_array[19] + (byte_array[18] << 8);
	pms7003.raw2_5 = byte_array[21] + (byte_array[20] << 8);
	pms7003.raw5_0 = byte_array[23] + (byte_array[22] << 8);
	pms7003.raw10_0 = byte_array[25] + (byte_array[24] << 8);
	pms7003.error = 1; //positive denotes no error.
	return pms7003;

}

bool checksum_valid(uint8_t byte_array[])
{
	short checksum_calculate = 0;
	short checksum_value = byte_array[31] + (byte_array[30] << 8);
	// sum the next 27 bytes
	for (int ii = 0; ii < 30; ii++)
	{
		checksum_calculate += byte_array[ii];

	}
	return checksum_value == checksum_calculate;
}


bool got_header(uint8_t byte_array[], uint8_t *data_buffer)
{
	for (int ii = 0; ii < (sizeof(byte_array) / sizeof(byte_array[0])); ii++)
	{
		if (byte_array[ii] == 66 && byte_array[ii + 1] == 77)
		{
			// found the header, lets return the rest of the data
			for (int jj = 0; jj < 32; jj++)
			{
				data_buffer[jj] = byte_array[ii + jj];

			}
			return true;
		}
	}
	return false;
}



struct PMS7003 get_pms7003(int uart_fd)
{
	// defaults are mostly ok, just need to change baud rate

	// open config object

	// from datasheet, start of data is identified by 66,77

	int read_attempts = 0;


	int bytes_written = -1;
	bytes_written = write(uart_fd, activate_command, 7);
	memset(initial_buffer, 0, sizeof(initial_buffer));
	memset(initial_buffer, 0, sizeof(initial_buffer));
	bytes_read = 0;


	header_found = false;
	checksum_bool = false;
	start_time = time(NULL);
	end_time = time(NULL);
	while (!checksum_bool)
	{
		if ((end_time - start_time) > MAX_READ_SECONDS)
		{
			data_7003.error - 2;
			return data_7003;
		}
		bytes_read = read(uart_fd, initial_buffer, 64);
		end_time = time(NULL);
		if (bytes_read >= 32)
		{
			// I've noticed that the first few bytes might not be the headers.
			// lets see where this actually begins
			if (got_header(initial_buffer, data_buffer))
			{
				// if we were able to get header, it fills data buffer
				checksum_bool = checksum_valid(initial_buffer);
				if (checksum_bool)
				{
					data_7003.error = 1;
					return parse_pms7003(initial_buffer);
				}
			}

		}
		read_attempts++;
	}
	data_7003.error = -3;
	return data_7003;
};