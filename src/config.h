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
	//The configuration database
	struct Config
	{
		Config(std::string const& filename);
		~Config();
		
		//Accessor methods
		std::string readString(std::string const& key);
		void storeString(std::string const& key, std::string const& value);
		
		int64_t readInt(std::string const& key);
		void storeInt(std::string const& key, int64_t value);
		
		long double readFloat(std::string const& key);
		void storeFloat(std::string const& key, long double value);
		
				
		//Resets the server with the default options
		void resetDefaults();
		
	private:
		
		//main config database
		TCHDB* config_db;
		
		//Loads the default config options
		void loadDefaults();
	};
};

#endif
