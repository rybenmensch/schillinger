inlets = 1;
outlets = 1;

var dict;
var shortDesc = "";
var longDesc = "";

function name(a){
	var list = arrayfromargs(arguments);
	d_name = list[0];
	//post(d_name );
	dict = max.getrefdict(d_name);
	if(typeof(dict) == "object"){
		shortDesc = dict.get("digest");
		longDesc = dict.get("description");
		dict.freepeer();
		outlet(0, longDesc);
	}
}

function init(){
	
}