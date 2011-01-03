
var Session = 
{
	logged_in: false,
	session_id: ""
};

Session.register = function(username, password)
{
	if(!Session.valid_username(username))
		return 'Invalid user name';

	var password_hash = Sha256.hash(password);
	

	var XHR = new XMLHttpRequest();
	XHR.open("GET", "r?n="+username+"&p="+password_hash, false);
	XHR.send(null);
	var response = XHR.responseText;
	
	alert(response);
	
	return 'ok';
}

Session.login = function(username, password)
{
	if(!Session.valid_username(username))
		return 'Invalid user name';

	var password_hash = Sha256.hash(password);
	
	var XHR = new XMLHttpRequest();
	XHR.open("GET", "l?n="+username+"&p="+password_hash, false);
	XHR.send(null);
	var response = XHR.responseText;
	
	alert(response);
	
	return 'ok';
}

Session.logout = function()
{
	alert('foo');

	if(logged_in)
	{		
		alert('what');
	
		var XHR = new XMLHttpRequest();
		XHR.open("GET", "q?k="+session_id, false);
		XHR.send(null);
		var response = XHR.responseText;
		
		alert(response);
		
		logged_in = false;
		session_id = false;	
	}
}

Session.valid_username = function(username)
{
	return 	username.length >= 3 && 
			username.length <= 20 &&
			/^\w+$/i.test(username)
}

