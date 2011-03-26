"use strict";

var Session = 
{
	logged_in: false,
	session_id: null,
	user_name: "",
	password_hash : "",
	player_name : "",
	character_names : [],
	
	hash_password : function(user_name, password)
	{
		return Sha256.hash(user_name + password + "qytrh1nz");
	},
	
	server_action : function(type, character_name)
	{
		var pbuf = new Network.ClientPacket;
		pbuf.login_packet = new Network.LoginRequest;
		pbuf.login_packet.user_name = Session.user_name;
		pbuf.login_packet.password_hash = Session.password_hash;
		pbuf.login_packet.action = type;

		if(typeof(character_name) != "undefined")
		{
			pbuf.login_packet.character_name = character_name;
		}
		
		var response = sendProtoBuf_sync(pbuf);
		if(response == null)
		{
			return "Could not reach server";
		}
		
		if(response.error_response)
		{
			return response.error_response.error_message;
		}
		
		if(typeof(response.login_response) == "undefined" ||
			!(response.login_response.success))
		{
			return "Request failed for some unspecified reason";
		}
		
		if(typeof(response.login_response.session_id) != "undefined")
		{
			Session.session_id = response.login_response.session_id;
			Session.logged_in = true;
		}
		
		if(typeof(response.login_response.character_names) != "undefined")
		{
			Session.character_names = new Array(response.login_response.character_names.length);
			for(var i=0; i<Session.character_names.length; ++i)
			{
				Session.character_names[i] = response.login_response.character_names[i];
			}
		}
		
		return "Ok";
	},

	create_account : function(user_name, password)
	{
		if(!Session.valid_user_name(user_name))
			return "Invalid user name";
		if(!Session.valid_password(password))
			return 'Password is too short';
		Session.user_name = user_name;
		Session.password_hash = Session.hash_password(user_name, password);
		return Session.server_action(Network.LoginRequest.LoginAction.CreateAccount);
	},
	
	delete_account : function(user_name, password)
	{	
		Session.user_name = user_name;
		Session.password_hash = Session.hash_password(user_name, password);
		return Session.server_action(Network.LoginRequest.LoginAction.DeleteAccount);
	},

	login : function(user_name, password)
	{
		Session.user_name = user_name;
		Session.password_hash = Session.hash_password(user_name, password);
		return Session.server_action(Network.LoginRequest.LoginAction.Login);
	},
	
	create_character : function(character_name)
	{
		return Session.server_action(Network.LoginRequest.LoginAction.CreateCharacter, character_name);
	},
	
	delete_character : function(character_name)
	{
		return Session.server_action(Network.LoginRequest.LoginAction.DeleteCharacter, character_name);	
	},
	
	join_game : function(character_name)
	{
		var res = Session.server_action(Network.LoginRequest.LoginAction.Join, character_name);
		
		if(res == "Ok")
		{
			Session.user_name = "";
			Session.password_hash = "";
		}
		
		return res;
	},

	logout : function()
	{
		//FIXME: Implement this
	},

	valid_user_name : function(user_name)
	{
		return 	user_name.length >= 3 && 
				user_name.length <= 20 &&
				/^\w+$/i.test(user_name)
	},

	valid_password : function(password)
	{
		return password.length > 6;
	}
}

