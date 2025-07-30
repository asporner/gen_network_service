	//*********************************************************
	//*  \fn             jsCallAjax
	//*********************************************************
function jsCallAjax(parms, callback) {

	var uuid = sessionStorage.getItem("session");
	
	if (uuid == null) {
    	var d = new Date().getTime();
    	uuid = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
        	var r = (d + Math.random()*16)%16 | 0;
        	d = Math.floor(d/16);
        	return (c=='x' ? r : (r&0x3|0x8)).toString(16);
    	});
		sessionStorage.setItem("session", uuid);
	}

	parms = uuid + ';' + parms;

	xhr = new XMLHttpRequest();

	xhr.open("POST", "/api/submit", true); // relative path via reverse proxy
	xhr.setRequestHeader("Content-Type", "application/json");

	xhr.onreadystatechange = function() {
		if (this.readyState == 4) {
			if (this.status == 200) {
				if (typeof callback == 'function') {
					callback(this);
				}
			}
		}

		return;
	}

	xhr.send(parms);

	return;
}

    //*********************************************************
    //*  \fn    AjaxInitCB
    //*********************************************************
function AjaxLoadCB(ret_obj) 
{
	if (ret_obj.responseText.length) {
		obj = document.getElementById('OUTPUT_FRAME');
		obj.innerHTML = ret_obj.responseText;
	}
}

    //*********************************************************
    //*  \fn    jsCregCB
    //*********************************************************
function jsAjaxLoad(opts) {
	jsCallAjax('TRANSACTION_NAME', AjaxLoadCB);
}

