#ifndef LOGIN_H
#define LOGIN_H

#include <string>

namespace Server
{
	void init_login();
	
	void checkpoint_login();

	bool verify_user_name(const std::string& name, const std::string& password_hash);

	bool create_account(const std::string& name, const std::string& password_hash);

	void delete_account(const std::string& name);
};

#endif

