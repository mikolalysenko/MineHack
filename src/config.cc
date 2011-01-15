#include "config.h"

using namespace std;

namespace Game
{


Config::Config(std::string const& filename)
{
	pthread_mutex_init(&config_lock, NULL);
	
	config_db = tchdbnew();
	
	tchdbsetmutex(config_db);
	tchdbtune(config_db, 0, 4, 10, HDBTBZIP);
	
	//Open the map database
	tchdbopen(config_db, filename.c_str(), HDBOWRITER | HDBOCREAT);
}

int Config::readInt(std::string const& key)
{
	int retval = 0;
	
	//int l = tchdbget3(config_db, (const void*)key.data, key.size, (void*)&retval, sizeof(int));
	
	return retval;
}

void Config::storeInt(int i, std::string const& key)
{
	
}

void Config::shutdown()
{
	tchdbclose(config_db);
	tchdbdel(config_db);
}

};