#include "config.h"

using namespace std;

namespace Game
{


Config::Config(std::string const& filename)
{
	config_db = tchdbnew();
	
	tchdbsetmutex(config_db);
	tchdbtune(config_db, 0, 4, 10, HDBTBZIP);
	
	//Open the map database
	tchdbopen(config_db, filename.c_str(), HDBOWRITER | HDBOCREAT);
}


Config::~Config()
{
	tchdbclose(config_db);
	tchdbdel(config_db);
}

int64_t Config::readInt(std::string const& key)
{
	int64_t retval = 0;
	int l = tchdbget3(config_db, (const void*)key.data(), key.size(), (void*)&retval, sizeof(int64_t));
	
	if(l != sizeof(int64_t))
		return 0;
	
	return retval;
}

void Config::storeInt(int64_t i, std::string const& key)
{
	tchdbput(config_db,
		(const void*)key.data(), key.size(),
		(void*)&i, sizeof(int64_t));
}


};
