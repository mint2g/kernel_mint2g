var header = function() {
/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 ************************************************
 * Automatically generated C config: don't edit *
 ************************************************
 */
}

Function.prototype.getMultiLine = function() {
    var lines = new String(this);
    lines = lines.substring(lines.indexOf("/*") + 2, lines.lastIndexOf("*/"));
    return lines;
}

String.prototype.format= function() {
	var args = arguments;
	return this.replace(/\{(\d+)\}/g, function(s, i) {
		return args[i];
	});
}

function parseBits(str) {
	var arr = new Array(32);
	for(i = 0; i < arr.length; i++) arr[i] = '0';

	var v = 0;
	for (i = 0; i < str.length; i++) {
		c = str.charAt(i);
		if (c == ',') {
			arr[v] = '1';v = 0;
		}
		else if (c == ':') {
			arr[v] = '|';v = 0;
		}
		else {
			v = v * 10 + parseInt(c);
		}
	}
	arr[v] = '|';

	for (i = 0; i < arr.length; i++) {
		if (arr[i] == '|') {
			arr[i] = '1';break;
		}
	}
	for (;i < arr.length; i++) {
		c = arr[i];arr[i] = '1';
		if (c == '|') break;
	}
	return arr;
}

function padLeft(str, lenght, padding) {
	if (str.length >= lenght) return str;
	else return padLeft(padding + str, lenght, padding);
}

function padRight(str, lenght, padding) {
	if (str.length >= lenght) return str;
	else return padRight(str + padding, lenght, padding);
}

//console.log('hi');
//console.log(process.argv[2]);
//return;

var align = 40;
var resbuf = '/*' + header.getMultiLine() + '*/\n\n';
var filename = process.argv[2];
var json;

var fs = require('fs');
fs.readFile(filename, function(err, data) {
	if (err) throw err;
	//console.log(data);
	json = JSON.parse(data);
	//console.log('here');

	//controller desc
	if (filename.indexOf(json.ctrl.name) == -1) console.log("Warning: invalid controller name!!!");
	var ctl_name = 'CTL_' + json.ctrl.name.toUpperCase();
	if (json.ctrl.die >= 2/* see below */) var ana_ctl_name = 'ANA_' + ctl_name;//mixed controller
	if (json.ctrl.die == 2/* only adie */) ctl_name = 'ANA_CTL_' + json.ctrl.name.toUpperCase();
	//console.log(json.ctrl.name);
	resbuf += '#ifndef __{0}_H__\n#define __{1}_H__\n\n'.format(ctl_name.toUpperCase(), ctl_name.toUpperCase());
	resbuf += '#define ' + ctl_name + '\n';
	if (ana_ctl_name) resbuf += '#define ' + ana_ctl_name + '\n';
	resbuf += '\n';

	var regsbuf = '/* registers definitions for controller {0} */\n'.format(ctl_name);
	var bitsbuf = '';
	var ana_regsbuf = '';
	if (ana_ctl_name) ana_regsbuf += '/* registers definitions for controller {0} */\n'.format(ana_ctl_name);
	//register desc
	for (var i in json.ctrl.regs) {
		//console.log(json.ctrl.regs[i].name + ' ' + json.ctrl.regs[i].offset);
		var reg_name = 'REG_' + json.ctrl.name.toUpperCase() + '_' + json.ctrl.regs[i].name.toUpperCase();
		if (ana_ctl_name) var ana_reg_name = 'ANA_' + reg_name;
		if (json.ctrl.regs[i].comment) regsbuf += '/* {0} */\n'.format(json.ctrl.regs[i].comment);
		regsbuf += padRight('#define ' + reg_name, align, ' ');
		regsbuf += 'SCI_ADDRESS({0}_BASE, {1})'.format(ctl_name, json.ctrl.regs[i].offset) + '\n';
		if (ana_reg_name) {
			ana_regsbuf += padRight('#define ' + ana_reg_name, align, ' ');
			ana_regsbuf += 'SCI_ADDRESS({0}_BASE, {1})'.format(ana_ctl_name, json.ctrl.regs[i].offset) + '\n';
		}

		if (json.ctrl.regs[i].bits) bitsbuf += '\n/* bits definitions for register {0} */\n'.format(reg_name);
		var miscbuf = '';
		for (var j in json.ctrl.regs[i].bits) {
			//console.log('\t' + json.ctrl.regs[i].bits[j].name + ' ' + json.ctrl.regs[i].bits[j].offset);
			if (json.ctrl.regs[i].bits[j].comment) bitsbuf += '/* {0} */\n'.format(json.ctrl.regs[i].bits[j].comment);
			var str = json.ctrl.regs[i].bits[j].offset;
			if (str.indexOf(':') == -1) {
				bitsbuf += padRight('#define BIT_' + json.ctrl.regs[i].bits[j].name.toUpperCase(), align, ' ');
				bitsbuf += '( BIT({0}) )\n'.format(str);
			}
			else {
				arr = parseBits(str);
				//console.log(arr.toString());
				msk = '';
				for (var k = 0; k < arr.length; k++) {
					if (arr[k] == '1') {
						(msk == '')?msk = 'BIT(' + k + ')':msk += '|' + 'BIT(' + k + ')';
					}
				}
				//console.log(msk);
				bits_name = 'BITS_' + json.ctrl.regs[i].bits[j].name.toUpperCase();
				bitsbuf += padRight('#define ' + bits_name + '(_x_)', align, ' ');
				bitsbuf += '( (_x_) << {0} & ({1}) )'.format(arr.toString().indexOf('1') / 2, msk) + '\n';
				if (json.ctrl.regs[i].bits[j].misc) {
					miscbuf += '\n';
					miscbuf += padRight('#define ' + 'SHIFT_'+ json.ctrl.regs[i].bits[j].name.toUpperCase(), align, ' ') + '( ' + arr.toString().indexOf('1') / 2 + ' )\n';;
					miscbuf += padRight('#define ' + 'MASK_' + json.ctrl.regs[i].bits[j].name.toUpperCase(), align, ' ') + '( ' + msk + ' )\n';;
				}
			}
		}
		bitsbuf += miscbuf;
	}

	var varsbuf = '';
	varsbuf += '\n/* vars definitions for controller {0} */\n'.format(ctl_name);
	for (var i in json.ctrl.vars) {
		varsbuf += padRight('#define ' + /*json.ctrl.name.toUpperCase() + '_' + */ json.ctrl.vars[i].name.toUpperCase(), align, ' ');
		varsbuf += '( {0} )\n'.format(json.ctrl.vars[i].value);
	}

	if (json.ctrl.die == 2/* only adie */) regsbuf = '';
	resbuf += regsbuf + ana_regsbuf + bitsbuf + varsbuf;
	resbuf += '\n#endif //__{0}_H__\n'.format(ctl_name.toUpperCase());
	outfilename = ctl_name.toLowerCase() + '.h';
	fs.writeFile(outfilename, resbuf, function(err) {
		if (err) throw err;
		console.log('Generating {0} to {1} ok'.format(filename, outfilename));
		if (process.argv[3]) gen_mapfile(process.argv[3]);
	});
});

function gen_mapfile(filename) {
	console.log(filename);
	fs.readFile(filename, function(err, data) {
		if (err) throw err;
		//console.log(data);
		var jsmap = JSON.parse(data);
		//console.log('here');
		var resbuf = '';
		for (var i in jsmap.maps) {
			//console.log(jsmap.maps[i].name);
			if (jsmap.maps[i].name != json.ctrl.name) continue;
			for (var j in jsmap.maps[i].items) {
				//console.log(jsmap.maps[i].items[j].offset + ':' + jsmap.maps[i].items[j].value);
				for(var k in json.ctrl.regs) {
					if (json.ctrl.regs[k].offset == jsmap.maps[i].items[j].offset) break;
				}
				//console.log(json.ctrl.regs[k].name);
				var reg_name = json.ctrl.regs[k].name;
				var bits = padRight(parseInt(jsmap.maps[i].items[j].value, 16).toString(2).split("").reverse().join("").toString(), 32, '0');
				//console.log(bits);
				var slp_io = 'slp_z,slp_oe,slp_ie,slp_inv'.split(",")[parseInt(bits[1]+bits[0], 2)];
				var slp_pull = 'slp_nul,slp_wpd,slp_wpu,slp_inv'.split(",")[parseInt(bits[3]+bits[2], 2)];
				var sel_mode = parseInt(bits[5]+bits[4], 2);
				var pin_pull = 'nul,wpd,wpu,inv'.split(",")[parseInt(bits[7]+bits[6], 2)];
				var drv_strength = parseInt(bits[9]+bits[8], 2);
				resbuf += padRight('{REG_PIN_{0},'.format(reg_name).toUpperCase(), 30, ' ');
				resbuf += 'BITS_PIN_DS({0})|BITS_PIN_AF({1})|BIT_PIN_{2}|BIT_PIN_{3}|BIT_PIN_{4}},\n'.format(drv_strength, sel_mode, pin_pull, slp_pull, slp_io).toUpperCase();
			}
		}
		//console.log(resbuf);
		outfilename = '__{0}map.c'.format(json.ctrl.name.toLowerCase());
		fs.writeFile(outfilename, resbuf, function(err) {
			if (err) throw err;
			console.log('Generating {0} to {1} ok'.format(filename, outfilename));
		});
	});
}

/*
 notes:
 1. die map:
	value 	adie	ddie
	0				*
	1				*
	2		*
	3 		*		*
 */
