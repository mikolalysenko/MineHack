#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <cstdint>
#include <cstdlib>

#include <pthread.h>

#include <tcutil.h>
#include <tchdb.h>

namespace Game
{
	struct Config
	{
		Config(std::string const& filename);
		
		//closes the database
		void shutdown();
		
		//reading functions to get values from the database
		int readInt(std::string const& key);
		
		//storing functions to store into the database
		void storeInt(int i, std::string const& key);
		
		private:
		
			//main config database
			TCHDB* config_db;
		
			//lock the database to prevent multiple writes / reading while writing
			pthread_mutex_t config_lock;
	};
};

#endif