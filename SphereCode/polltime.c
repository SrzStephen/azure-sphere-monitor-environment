#include <applibs/storage.h>
#include <applibs/log.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t lock;

bool init_lock()
{
	pthread_mutex_init(&lock, NULL);
}

bool change_polling_time(int polltime)
{
	if (pthread_mutex_trylock(&lock) != 0)
	{
		Log_Debug("Couldn't open lock when writing");
		return false;
	}
	int fd_file = -3;
	fd_file = Storage_OpenMutableFile();
	if (fd_file < 0)
	{
		Log_Debug("couldn't open file to write");
		pthread_mutex_unlock(&lock);
		return false;
	}
	ssize_t written = write(fd_file, &polltime, sizeof(polltime));
	if (written != sizeof(polltime))
	{
		Log_Debug("Issue writing to file");
		close(fd_file);
		pthread_mutex_unlock(&lock);
		return false;
	}
	close(fd_file);
	pthread_mutex_unlock(&lock);
	return true;
}


int get_polling_time()
{
	if (pthread_mutex_trylock(&lock) != 0)
	{
		Log_Debug("Couldn't open lock when reading");
		return -1;
	}
	int fd_file = -3;
	fd_file = Storage_OpenMutableFile();
	if (fd_file < 0)
	{
		Log_Debug("couldn't open file to read");
		pthread_mutex_unlock(&lock);
		close(fd_file);
		pthread_mutex_unlock(&lock);
		return -2;
	}
	int polltime = 0;
	ssize_t readsize = read(fd_file, &polltime, sizeof(polltime));
	close(fd_file);
	pthread_mutex_unlock(&lock);
	if (readsize < sizeof(polltime))
	{

		return -3;
	}
	Log_Debug("Polling time from mutable storage is %i \n", polltime);
	return polltime;
}