
var Session = 
{
	//logged_in: true,
	
	logged_in: false,
	session_id: ""
};

Session.hash_password = function(username, password)
{
	return Sha256.hash(username + password + "qytrh1nz");
}

Session.handle_login = function(XHR)
{
	XHR.send(null);
	var response = XHR.responseText;
	
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
		Session.session_id = false;	
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

