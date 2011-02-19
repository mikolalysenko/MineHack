"use strict";

var Session = 
{
	logged_in: false,
	session_id: "",
	player_name: ""
};

Session.hash_password = function(username, password)
{
	return Sha256.hash(username + password + "qytrh1nz");
}

Session.get_session_id_arr = function()
{
	var arr = new Uint8Array(8);
	var x 	= parseInt(Session.session_id.substr(8, 8), 16);
	var y 	= parseInt(Session.session_id.substr(0, 8), 16);
		
	arr[0] = (x)	 	& 0xff;
	arr[1] = (x >> 8) 	& 0xff;
	arr[2] = (x >> 16) 	& 0xff;
	arr[3] = (x >> 24) 	& 0xff;
	arr[4] = (y) 		& 0xff;
	arr[5] = (y >> 8) 	& 0xff;
	arr[6] = (y >> 16) 	& 0xff;
	arr[7] = (y >> 24) 	& 0xff;

	return arr;
}

Session.handle_login = function(XHR)
{
	XHR.send(null);
	var response = XHR.responseText,
		lines = response.split('\n');
	if(lines.length == 2 && lines[0] == 'Ok')
	{
		Session.logged_in = true;
		Session.session_id = lines[1];
		return 'ok';
	}
	else if(lines.length == 2)
	{
		return lines[1];
	}
	return "Unknown error";
}


Session.register = function(username, password)
{
	if(!Session.valid_username(username))
		return 'Invalid user name';
		
	if(!Session.valid_password(password))
		return 'Password is too short';

	var password_hash = Session.hash_password(username, password);
	var XHR = new XMLHttpRequest();
	XHR.open("GET", "r?n="+username+"&p="+password_hash, false);

	return Session.handle_login(XHR);
}

Session.login = function(username, password)
{
	if(!Session.valid_username(username))
		return 'Invalid user name';

	var password_hash = Session.hash_password(username, password);
	var XHR = new XMLHttpRequest();
	XHR.open("GET", "l?n="+username+"&p="+password_hash, false);
	
	return Session.handle_login(XHR);
}

Session.logout = function()
{
	if(Session.logged_in)
	{		
		var XHR = new XMLHttpRequest();
		XHR.open("GET", "q?k="+session_id, false);
		XHR.send(null);
		
		Session.logged_in = false;
		Session.session_id = "";	
		Session.player_name = "";
	}
}

Session.valid_username = function(username)
{
	return 	username.length >= 3 && 
			username.length <= 20 &&
			/^\w+$/i.test(username)
}

Session.valid_password = function(password)
{
	return password.length > 6;
}

Session.valid_player_name = function(player_name)
{
	return 	player_name.length <= 20 &&
			/^\w+$/i.test(player_name)
}

Session.do_action = function(url)
{
	var XHR = new XMLHttpRequest();
	XHR.open("GET", url, false);
	XHR.send();
	
	return XHR.responseText.split("\n");
}


Session.get_players = function()
{
	if(!Session.logged_in || Session.player_name != "")
		return [];
	
	var response = Session.do_action("t?k="+Session.session_id);
	
	if(response[0] != "Ok")
		return [];
		
	return response.slice(1, response.length-1);
}

Session.add_player = function(player_name)
{
	if(player_name == "")
		return ["Fail", "Must enter player name first"];
		
	if(!Session.valid_player_name(player_name))
		return ["Fail", "Invalid player name"];

	if(!Session.logged_in || Session.player_name != "")
		return ["Fail", "Not logged in"];
		
	return Session.do_action("c?k="+Session.session_id+"&player_name="+player_name);
}

Session.remove_player = function(player_name)
{
	if(!Session.logged_in || Session.player_name != "")
		return ["Fail", "Not logged in"];
	
	return Session.do_action("d?k="+Session.session_id+"&player_name="+player_name);
}

Session.join_game = function(player_name)
{
	if(!Session.logged_in || Session.player_name != "")
		return ["Fail", "Not logged in"];
		
	var response = Session.do_action("j?k="+Session.session_id+"&player_name="+player_name);
	
	if(response[0] != "Ok")
		return ["Fail", response[1]];
		
	Session.player_name = player_name;
	Player.set_entity_id(response[2]);
	
	return ["Ok", "Successfully joined game"];
}


