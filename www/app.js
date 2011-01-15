//Application state

var LoginState = {};

LoginState.init = function()
{
	//Initialize the login state
	var loginPane = document.getElementById('loginElem');
	loginPane.style.display = 'block';
	
	Session.logout();
}

LoginState.shutdown = function()
{
	//Shutdown the login state
	var loginPane = document.getElementById('loginElem');
	loginPane.style.display = 'none';

	var pass_txt = document.getElementById('password');
	pass_txt.value = "";
	
	//Clear error message
	document.getElementById('errMsg').innerHTML = "";
}

LoginState.do_register = function()
{
	var user_txt = document.getElementById('userName');
	var pass_txt = document.getElementById('password');
	
	var res = Session.register(user_txt.value, pass_txt.value);
	pass_txt.value = "";
	
	if(res == 'ok')
	{
		App.set_state(CharacterSelectState);
	}
	else
	{
		App.state.do_login_error(res);
	}
}

LoginState.do_login = function()
{
	var user_txt = document.getElementById('userName');
	var pass_txt = document.getElementById('password');
	
	var res = Session.login(user_txt.value, pass_txt.value);
	pass_txt.value = "";
	
	if(res == 'ok')
	{
		App.set_state(CharacterSelectState);
	}
	else
	{
		App.state.do_login_error(res);
	}
}

LoginState.do_login_error = function(msg)
{
	//Scrub message
	msg = msg.replace(/\&/g, "&amp;")
			 .replace(/\</g, "&lt;")
			 .replace(/\>/g, "&gt;")
			 .replace(/\n/g, "\<br\/\>");
	
	//Set text
	document.getElementById('errMsg').innerHTML = msg;
}


//Character select state
var CharacterSelectState = {};

CharacterSelectState.init = function()
{
	//Regenerate players
	CharacterSelectState.player_list = [];
	CharacterSelectState.generate_players();

	var selectElem = document.getElementById("selectElem");
	selectElem.style.display = "block";	
}

CharacterSelectState.shutdown = function()
{
	var selectElem = document.getElementById("selectElem");
	selectElem.style.display = "none";
}

CharacterSelectState.do_select_player = function(player_num)
{
	var player_name = CharacterSelectState.players[player_num];
	var result		= Session.join_game(player_name);
	
	if(result[0] == "Ok")
	{
		App.set_state(LoadState);
	}
	else
	{
		alert(result[1]);
	}
}

CharacterSelectState.do_delete_player = function(player_num)
{
	var player_name = CharacterSelectState.players[player_num];
	var result 		= Session.remove_player(player_name);
	
	if(result[0] == "Ok")
	{
		CharacterSelectState.generate_players();
	}
	else
	{
		alert(result[1]);
	}
}

CharacterSelectState.do_create_player = function()
{
	var player_name	= document.getElementById("playerName");
	var result		= Session.add_player(player_name.value);
	
	if(result[0] == "Ok")
	{
		CharacterSelectState.generate_players();
		player_name.value = "";
	}
	else if(result.length > 1)
	{
		alert(result[1]);
	}
}


CharacterSelectState.generate_players = function()
{
	var listElem = document.getElementById("avatarList");
	listElem.innerHTML = "";

	CharacterSelectState.players = Session.get_players();
	
	for(var i in CharacterSelectState.players)
	{
		var player_name = CharacterSelectState.players[i];
		
		var div_elem = 
			'<a class="avatarSelect" href="javascript:App.state.do_select_player(' + i + ');">' + player_name + '</a>' + 
			'<input class="avatarDel" onclick="javascript:App.state.do_delete_player(' + i + ');" type="button" value = "X" />' +
			'<br/>';
			
		listElem.innerHTML += div_elem;
	}
}



//Application crash state
var ErrorState = {};

ErrorState.init = function()
{
	document.getElementById('errorElem').style.display = 'block';
}

ErrorState.shutdown = function()
{
	document.getElementById('errorElem').style.display = 'none';
}

ErrorState.post_error = function(msg)
{
	//Scrub message
	msg = msg.replace(/\&/g, "&amp;")
			 .replace(/\</g, "&lt;")
			 .replace(/\>/g, "&gt;")
			 .replace(/\n/g, "\<br\/\>");
			 
	document.getElementById('errorReason').innerHTML = msg;
}


//Waiting for resources to load state
var LoadState = {};

LoadState.init = function()
{
	document.getElementById('progress').style.display = 'block';
	
	if(Loader.finished)
	{
		App.set_state(GameState);
	}
}

LoadState.shutdown = function()
{
	document.getElementById('progress').style.display = 'none';
}

LoadState.update_progress = function(url)
{
	var prog_txt = document.getElementById('progress');
	prog_txt.innerHTML = "Loaded: " + url + "<br\/\>%" + Loader.pct_loaded * 100.0 + " Complete";
	
	if(Loader.finished && App.state == LoadState)
	{
		App.set_state(GameState);
	}
}


//Actual gameplay state
var GameState = {};

GameState.init = function()
{
	var appElem = document.getElementById('appElem');
	appElem.style.display = 'block';

	var res = Game.init(document.getElementById('canvas'));
	if(res != 'Ok')
	{
		App.crash(res);
	}
}

GameState.shutdown = function()
{
	var appElem = document.getElementById('appElem');
	appElem.style.display = 'hidden';

	Game.shutdown();
}


//The default state (doesn't do anything)
var DefaultState = {}

DefaultState.shutdown = function()
{
}


var App = 
{
	state : DefaultState
};

App.do_test = function()
{
	//Generate dummy account
	var user_txt = document.getElementById('userName');
	var pass_txt = document.getElementById('password');
	
	user_txt.value = "user" + Math.floor(Math.random()*10000000);
	pass_txt.value = "p" + Math.floor(Math.random()*10000000) + "." + Math.floor(Math.random()*10000000);
	
	App.state.do_register();
	
	//Create player
	var player_txt = document.getElementById('playerName');
	player_txt.value = "player" + Math.floor(Math.random()*10000000);
	App.state.do_create_player();
	
	//Join game
	App.state.do_select_player(0);
}


App.init = function()
{
	Loader.start(LoadState.update_progress, App.crash);
	App.set_state(LoginState);
	
	
	//TODO: take this out for production
	//App.do_test();
}

App.shutdown = function()
{
	App.set_state(DefaultState);
}

App.set_state = function(next_state)
{
	App.state.shutdown();
	App.state = next_state;
	App.state.init();
}

App.crash = function(msg)
{
	App.set_state(ErrorState);	
	App.state.postError(msg);
}



