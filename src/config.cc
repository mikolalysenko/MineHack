
#include <sstream>

#include "constants.h"
#include "config.h"
#include "misc.h"

using namespace std;

namespace Game
{

Config::Config(std::string const& filename)
{
	config_db = tchdbnew();
	
	tchdbsetmutex(config_db);
	tchdbtune(config_db, 0, 4, 10, HDBTBZIP);
	tchdbsetcache(config_db, (1<<24));
	tchdbsetxmsiz(config_db, (1<<24));
	
	//Open the map database
	if(!tchdbopen(config_db, filename.c_str(), HDBOWRITER))
	{
		tchdbopen(config_db, filename.c_str(), HDBOWRITER | HDBOCREAT);
		resetDefaults();
	}
}

Config::~Config()
{
	tchdbclose(config_db);
	tchdbdel(config_db);
}

std::string Config::readString(std::string const& key)
{
	char* str = tchdbget2(config_db, key.c_str());
	if(str == NULL)
		return "";
	
	auto G = ScopeFree(str);
	return str;
}

void Config::storeString(std::string const& key, std::string const& value)
{
	tchdbput2(config_db, key.c_str(), value.c_str());
}

int64_t Config::readInt(string const& key)
{
	stringstream ss(readString(key));
	int64_t res;
	ss >> res;
	return res;
}

void Config::storeInt(string const& key, int64_t value)
{
	stringstream ss;
	ss << value;
	storeString(key, ss.str());
}

long double Config::readFloat(string const& key)
{
	stringstream ss(readString(key));
	long double res;
	ss >> res;
	return res;
}

void Config::storeFloat(string const& key, long double value)
{
	stringstream ss;
	ss << value;
	storeString(key, ss.str());
}

		
//Set default values for the config database
void Config::resetDefaults()
{
	tchdbvanish(config_db);
	
	//Default state variables
	storeInt("ticks", 1);
	
	//HttpServer defaults
	storeString("wwwroot", "www");
	storeInt("listenport", 8081);
	storeString("origin", "127.0.0.1:8081");
	
	//Database paths
	storeString("login_db_path", "data/login.tch");
	storeString("map_db_path", "data/map.tch");
	
	//Performance tweaks
	storeInt("visible_radius", 16);
	storeFloat("update_rate", 0.125);
	storeFloat("map_db_write_rate", 1.0);
	storeFloat("session_timeout", 10000.0);
	storeInt("num_chunk_buckets", (1<<20));
	storeInt("num_surface_chunk_buckets", (1<<20));
	storeInt("tc_map_buckets", (1<<20));
	storeInt("tc_map_alignment", 4);
	storeInt("tc_map_free_pool_size", 10);
	storeInt("tc_map_cache_size", 10 * (1<<20));
	storeInt("tc_map_extra_memory", 128 * (1<<20));
}

};
