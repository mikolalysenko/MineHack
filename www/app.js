"use strict";

//The Login state
var LoginState = {

	init : function()
	{
		document.getElementById('loginPane').style.display = 'block';
		document.getElementById("loginButton").onclick = LoginState.do_login;
		document.getElementById("createButton").onclick = LoginState.do_create;
	},

	shutdown : function()
	{
		document.getElementById('loginPane').style.display = 'none';
		document.getElementById('password').pass_txt.value = "";
		document.getElementById('loginError').innerHTML = "";
	},

	do_create : function()
	{
		var user_txt = document.getElementById('userName');
		var pass_txt = document.getElementById('password');
	
		var res = Session.create_account(user_txt.value, pass_txt.value);
		pass_txt.value = "";
	
		if(res == "Ok")
		{
			App.set_state(CharacterSelectState);
		}
		else
		{
			LoginState.set_login_error(res);
		}
	},

	do_login : function()
	{
		var user_txt = document.getElementById('userName');
		var pass_txt = document.getElementById('password');
	
		var res = Session.login(user_txt.value, pass_txt.value);
		pass_txt.value = "";
	
		if(res == "Ok")
		{
			App.set_state(CharacterSelectState);
		}
		else
		{
			LoginState.set_login_error(res);
		}
	},

	set_login_error : function(msg)
	{
		//Scrub message
		msg = msg.replace(/\&/g, "&amp;")
				 .replace(/\</g, "&lt;")
				 .replace(/\>/g, "&gt;")
				 .replace(/\n/g, "\<br\/\>");
	
		//Set text
		document.getElementById('loginError').innerHTML = msg;
	}
};

//Application crash state
var ErrorState = {

	init : function()
	{
		document.getElementById('errorPane').style.display = 'block';
	},

	shutdown : function()
	{
		document.getElementById('errorPane').style.display = 'none';
	},

	post_error : function(msg)
	{
		//Scrub message
		msg = msg.replace(/\&/g, "&amp;")
				 .replace(/\</g, "&lt;")
				 .replace(/\>/g, "&gt;")
				 .replace(/\n/g, "\<br\/\>");
				 
		document.getElementById('errorReason').innerHTML = msg;
	}
}

//The default state (doesn't do anything)
var DefaultState = {

	init : function() { },
	shutdown : function() { }
};


//The application object
var App = {
	state : DefaultState,
	
	init : function()
	{
		//Start loading resources
		//Loader.start(LoadState.update_progress, App.crash);
		App.set_state(LoginState);
	},

	shutdown : function()
	{
		App.set_state(DefaultState);
	},

	set_state : function(next_state)
	{
		App.state.shutdown();
		App.state = next_state;
		App.state.init();
	},

	crash : function(msg)
	{
		App.set_state(ErrorState);	
		App.state.post_error(msg);
	}
};


