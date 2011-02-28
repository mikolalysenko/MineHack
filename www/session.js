"use strict";

var Session = 
{
	logged_in: false,
	session_id: null,
	user_name: "",
	
	hash_password : function(user_name, password)
	{
		return Sha256.hash(user_name + password + "qytrh1nz");
	},
	
	server_action : function(user_name, password, type)
	{
		if(!Session.valid_user_name(user_name))
			return 'Invalid user name';
		
		if(!Session.valid_password(password))
			return 'Password is too short';

		var pbuf = new Network.ClientPacket;
		pbuf.login_packet = new Network.LoginRequest;
		pbuf.login_packet.user_name = user_name;
		pbuf.login_packet.password_hash = Session.hash_password(user_name, password);
		pbuf.login_packet.action = type;
		
		var response = sendProtoBuf_sync(pbuf);
		if(response == null)
		{
			return "Could not reach server";
		}
		
		if(response.error_response)
		{
			return response.error_response.error_message;
		}
		
		//Success
		Session.logged_in = true;
		Session.session_id = response.login_response.session_id;
		Session.user_name = user_name;
		
		return "Ok";
	},

	create_account : function(user_name, password)
	{
		return Session.server_action(user_name, password, Network.LoginRequest.LoginAction.Create);
	},

	login : function(user_name, password)
	{
		return Session.server_action(user_name, password, Network.LoginRequest.LoginAction.Login);
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

