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
		document.getElementById('password').value = "";
		App.post_error("");
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
			App.post_error(res);
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
			App.post_error(res);
		}
	},

	post_error : function(msg)
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


//Character select state
var CharacterSelectState = {

	init : function()
	{
		document.getElementById("selectPane").style.display = "block";	
		document.getElementById("createCharacter").onclick = CharacterSelectState.do_create_character;
		CharacterSelectState.generate_list();
	},

	shutdown : function()
	{
		App.post_error("");
		document.getElementById("selectPane").style.display = "none";
	},
	
	post_error : function(msg)
	{
		msg = msg.replace(/\&/g, "&amp;")
				 .replace(/\</g, "&lt;")
				 .replace(/\>/g, "&gt;")
				 .replace(/\n/g, "\<br\/\>");
	
		document.getElementById('selectError').innerHTML = msg;
	},

	do_join_game : function(character_name)
	{
		App.post_error("");
		var result = Session.join_game(character_name);
	
		if(result == "Ok")
		{
			App.set_state(LoadState);
		}
		else
		{
			App.post_error(result);
		}
	},

	do_delete_character : function(character_name)
	{
		App.post_error("");
		var result = Session.delete_character(character_name);
	
		if(result == "Ok")
		{
			CharacterSelectState.generate_list();
		}
		else
		{
			App.post_error(result);
		}
	},

	do_create_character : function()
	{
		App.post_error("");
		var character_name	= document.getElementById("characterName");
		var result		= Session.create_character(character_name.value);
	
		if(result == "Ok")
		{
			CharacterSelectState.generate_list();
			character_name.value = "";
		}
		else
		{
			App.post_error(result);
		}
	},

	generate_list : function()
	{
		var listElem = document.getElementById("characterList"), i;
		listElem.innerHTML = "";

		for(i=0; i<Session.character_names.length; ++i)
		{
			var character_name = Session.character_names[i];
			listElem.innerHTML += 
				'<a class="avatarSelect" href="javascript:App.state.do_join_game(\'' + character_name + '\');">' + character_name + '</a>' + 
				'<input class="avatarDel" onclick="javascript:App.state.do_delete_character(\'' + character_name + '\');" type="button" value = "X" />' +
				'<br/>';
		}
	}
};


//The preloader
var LoadState = {

	init : function()
	{
		Game.preload();
	
		if(Loader.finished)
		{
			App.set_state(Game);
		}
		else
		{
			document.getElementById('progressPane').style.display = 'block';
		}
	},

	shutdown : function()
	{
		document.getElementById('progressPane').style.display = 'none';
	},

	update_progress : function(url)
	{
		var prog_txt = document.getElementById('progressPane');
		prog_txt.innerHTML = "Loaded: " + url + "<br\/\>%" + Loader.pct_loaded * 100.0 + " Complete";
	
		if(Loader.finished && App.state == LoadState)
		{
			App.set_state(Game);
		}
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
	
	
	do_test : function()
	{
		//Generate dummy account
		var user_txt = document.getElementById('userName');
		var pass_txt = document.getElementById('password');
	
		user_txt.value = "user" + Math.floor(Math.random()*10000000);
		pass_txt.value = "p" + Math.floor(Math.random()*10000000) + "." + Math.floor(Math.random()*10000000);
	
		App.state.do_create();
	
		//Create player
		var player_name = "player" + Math.floor(Math.random()*10000000);
		var player_txt = document.getElementById('characterName');
		player_txt.value = player_name;
		App.state.do_create_character();
	
		//Join game
		App.state.do_join_game(player_name);
	},
	
	init : function()
	{
		Loader.start(LoadState.update_progress, App.crash);
		App.set_state(LoginState);
		App.do_test();
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
	},
	
	post_error : function(msg)
	{
		App.state.post_error(msg);
	}
};


