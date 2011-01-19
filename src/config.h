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
		~Config();
		
		//reading functions to get values from the database
		int64_t readInt(std::string const& key);
		
		//storing functions to store into the database
		void storeInt(int64_t i, std::string const& key);
		
		private:
		
			//main config database
			TCHDB* config_db;
	};
};

#endif
