/*jslint strict: true, undef: true, onevar: true, evil: true, es5: true, adsafe: true, regexp: true, maxerr: 50, indent: 4 */
"use strict";

Loader = 
{
	shaders :
	[
		"shaders/chunk.vs",
		"shaders/chunk.fs",
		"shaders/vis.vs",
		"shaders/vis.fs",
		"shaders/simple.vs",
		"shaders/simple.fs",
		"shaders/shadow.fs",
		"shaders/shadow.vs"
	],
	
	images :
	[
		"img/terrain.png"
	],
	
	data : {},
	
	num_loaded : 0,
	max_loaded : 0,
	pct_loaded : 0,
	finished : false,
	failed : false
};



Loader.start = function(prog_callback, err_callback)
{
	Loader.max_loaded = Loader.shaders.length + Loader.images.length;

	Loader.onProgress = function(url)
	{
		++Loader.num_loaded;
		Loader.pct_loaded = Loader.num_loaded / Loader.max_loaded;
		Loader.finished = Loader.num_loaded >= Loader.max_loaded;
		prog_callback(url);
	}

	Loader.onError = function(data)
	{
		Loader.failed = true;
		err_callback("Error loading file: " + data);
	}

	for(var i=0; i<Loader.shaders.length; i++)
	{
		Loader.fetch_shader(Loader.shaders[i]);
	}	
	
	for(var i=0; i<Loader.images.length; i++)
	{
		Loader.fetch_image(Loader.images[i]);
	}
}

Loader.fetch_shader = function(url)
{
	var XHR = new XMLHttpRequest();
	XHR.open("GET", url, true);
	
	XHR.onreadystatechange = function()
	{
		if(XHR.readyState == 4)
		{
			if(XHR.status == 200 || XHR.status == 304)
			{
				Loader.data[url] = XHR.responseText;
				Loader.onProgress(url);
			}
			else
			{
				Loader.onError(url);
			}
		}
	}
	
	XHR.send(null);
}


Loader.fetch_image = function(url)
{
	var img = new Image();
	img.onload = function()
	{
		Loader.data[url] = img;
		Loader.onProgress(url);
	}
	img.src = url;
}
