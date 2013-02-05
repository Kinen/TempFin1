/*
 * config.c - configuration handling and persistence; master function table
 * Part of TinyG project
 *
 * Copyright (c) 2010 - 2013 Alden S. Hart Jr.
 *
 * TinyG is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * TinyG is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for details. You should have received a copy of the GNU General Public 
 * License along with TinyG  If not, see <http://www.gnu.org/licenses/>.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 *	Config system overview
 *
 *	--- Config objects and the config list ---
 *
 *	The config system provides a structured way to access and set configuration variables.
 *	It also provides a way to get an arbitrary variable for reporting. Config operates
 *	as a collection of "objects" (OK, so they are not really objects) that encapsulate
 *	each variable. The objects are collected into a list (the body), which also has  
 *	header and footer objects. This way the internals don't care about how the variable
 *	is represented or communicated externally as all operations occur on the cmdObj list. 
 *	The list is populated by the text_parser or the JSON_parser depending on the mode.
 *	The lists are also used for responses and are read out (printed) by a text-mode or
 *	JSON print function.
 */
/*	--- Config variables, tables and strings ---
 *
 *	Each configuration value is identified by a short mnemonic string (token). The token 
 *	is resolved to an index into the cfgArray which is an array of structures with the 
 *	static assignments for each variable. The array is organized as:
 * 
 *	- cfgArray contains typed data in program memory (PROGMEM). Each cfgItem has:
 *		- group string identifying what group the variable is part of (if any)
 *		- token string - the token for that variable - pre-pended with the group (if any)
 *		- operations flags - flag if the value should be initialized and/or persisted to NVM
 *		- pointer to a formatted print string also in program memory (Used only for text mode)
 *		- function pointer for formatted print() method or text-mode readouts
 *		- function pointer for get() method - gets values from memory
 *		- function pointer for set() method - sets values and runs functions
 *		- target - memory location that the value is written to / read from
 *		- default value - for cold initialization
 *
 *	Additionally an NVM array contains values persisted to EEPROM as doubles; indexed by cfgArray index
 *
 *	The following rules apply to mnemonic tokens
 *	 - are up to 5 alphnuneric characters and cannot contain whitespace or separators
 *	 - must be unique (non colliding).
 *	 - axis tokens start with the axis letter and are typically 3 characters including the axis letter
 *	 - motor tokens start with the motor digit and are typically 3 characters including the motor digit
 *	 - non-axis or non-motor tokens are 2-5 characters and by convention generally should not start 
 *		with: xyzabcuvw0123456789 (but there can be exceptions)
 *
 *  "Groups" are collections of values that mimic REST resources. Groups include:
 *	 - axis groups prefixed by "xyzabc"		("uvw" are reserved)
 *	 - motor groups prefixed by "1234"		("56789" are reserved)
 *	 - PWM groups prefixed by p1, p2 	    (p3 - p9 are reserved)
 *	 - coordinate system groups prefixed by g54, g55, g56, g57, g59, g92
 *	 - a system group is identified by "sys" and contains a collection of otherwise unrelated values
 *
 *	"Uber-groups" are groups of groups that are only used for text-mode printing - e.g.
 *	 - group of all axes groups
 *	 - group of all motor groups
 *	 - group of all offset groups
 *	 - group of all groups
 */
/*  --- Making changes and adding new values
 *
 *	Adding a new value to config (or changing an existing one) involves touching the following places:
 *
 *	 - Add a formatting string to fmt_XXX strings. Not needed if there is no text-mode print function
 *	   of you are using one of the generic print strings.
 * 
 *	 - Create a new record in cfgArray[]. Use existing ones for examples. You can usually use existing
 *	   functions for get and set; or create a new one if you need a specialized function.
 *
 *	   The ordering of group displays is set by the order of items in cfgArray. None of the other 
 *	   orders matter but are generally kept sequenced for easier reading and code maintenance. Also,
 *	   Items earlier in the array will resolve token searches faster than ones later in the array.
 *
 *	   Note that matching will occur from the most specific to the least specific, meaning that
 *	   if tokens overlap the longer one should be earlier in the array: "gco" should precede "gc".
 */
/*  --- Rules, guidelines and random stuff
 *
 *	It's the responsibility of the object creator to set the index. Downstream functions
 *	all expect a valid index. Set the index by calling cmd_get_index(). This also validates
 *	the token and group if no lookup exists.
 */
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>			// precursor for xio.h
#include <avr/pgmspace.h>	// precursor for xio.h

#include "kinen.h"			// config reaches into almost everything
#include "tempfin1.h"
#include "config.h"
#include "json_parser.h"
#include "report.h"
#include "util.h"
#include "system.h"
#include "xio/xio.h"
//#include "xmega/xmega_eeprom.h"

//#include "settings.h"
//#include "controller.h"
//#include "canonical_machine.h"
//#include "gcode_parser.h"
//#include "json_parser.h"
//#include "planner.h"
//#include "stepper.h"
//#include "gpio.h"
//#include "test.h"
//#include "help.h"

typedef char PROGMEM *prog_char_ptr;	// access to PROGMEM arrays of PROGMEM strings

//*** STATIC STUFF ***********************************************************

typedef struct cfgItem {
	char group[CMD_GROUP_LEN+1];		// group prefix (with NUL termination)
	char token[CMD_TOKEN_LEN+1];		// token - stripped of group prefix (w/NUL termination)
	uint8_t flags;						// operations flags - see defines below
	const char *format;					// pointer to formatted print string
	fptrPrint print;					// print binding: aka void (*print)(cmdObj_t *cmd);
	fptrCmd get;						// GET binding aka uint8_t (*get)(cmdObj_t *cmd)
	fptrCmd set;						// SET binding aka uint8_t (*set)(cmdObj_t *cmd)
	double *target;						// target for writing config value
	double def_value;					// default value for config item
} cfgItem_t;

// operations flags and shorthand
#define F_INITIALIZE	0x01			// initialize this item (run set during initialization)
#define F_PERSIST 		0x02			// persist this item when set is run
#define F_NOSTRIP		0x04			// do not strip to group from the token
#define _f00			0x00
#define _fin			F_INITIALIZE
#define _fpe			F_PERSIST
#define _fip			(F_INITIALIZE | F_PERSIST)
#define _fns			F_NOSTRIP
#define _f07			(F_INITIALIZE | F_PERSIST | F_NOSTRIP)

// prototypes are divided into generic functions and parameter-specific functions

// generic internal functions
static uint8_t _set_nul(cmdObj_t *cmd);	// noop
static uint8_t _set_ui8(cmdObj_t *cmd);	// set a uint8_t value
static uint8_t _set_int(cmdObj_t *cmd);	// set an integer value
static uint8_t _set_dbl(cmdObj_t *cmd);	// set a double value

static uint8_t _get_nul(cmdObj_t *cmd);	// get null value type
static uint8_t _get_ui8(cmdObj_t *cmd);	// get uint8_t value
static uint8_t _get_int(cmdObj_t *cmd);	// get uint32_t integer value
static uint8_t _get_dbl(cmdObj_t *cmd);	// get double value

static void _print_nul(cmdObj_t *cmd);	// print nothing
static void _print_str(cmdObj_t *cmd);	// print a string value
static void _print_ui8(cmdObj_t *cmd);	// print unit8_t value w/no units
static void _print_int(cmdObj_t *cmd);	// print integer value w/no units
static void _print_dbl(cmdObj_t *cmd);	// print double value w/no units

// helpers for generic functions
static char *_get_format(const index_t i, char *format);
//static uint8_t _text_parser(char *str, cmdObj_t *c);
static void _print_text_inline_pairs();
static void _print_text_inline_values();
static void _print_text_multiline_formatted();

static uint8_t _set_grp(cmdObj_t *cmd);	// set data for a group
static uint8_t _get_grp(cmdObj_t *cmd);	// get data for a group
//static void _do_group_list(cmdObj_t *cmd, char list[][CMD_TOKEN_LEN+1]); // helper to print multiple groups in a list

/*****************************************************************************
 **** PARAMETER-SPECIFIC CODE REGION *****************************************
 **** This code and data will change as you add / update config parameters ***
 *****************************************************************************/

// parameter-specific internal functions
//static uint8_t _get_id(cmdObj_t *cmd);	// get device ID (signature)
static uint8_t _set_hv(cmdObj_t *cmd);		// set hardware version
/*
static uint8_t _get_sr(cmdObj_t *cmd);		// run status report (as data)
static void _print_sr(cmdObj_t *cmd);		// run status report (as printout)
static uint8_t _set_sr(cmdObj_t *cmd);		// set status report specification
static uint8_t _set_si(cmdObj_t *cmd);		// set status report interval
static uint8_t _get_id(cmdObj_t *cmd);		// get device ID
static uint8_t _get_qr(cmdObj_t *cmd);		// run queue report (as data)
static uint8_t _get_rx(cmdObj_t *cmd);		// get bytes in RX buffer
static uint8_t _set_defa(cmdObj_t *cmd);	// reset config to default values

static uint8_t _set_ic(cmdObj_t *cmd);		// ignore CR or LF on RX input
static uint8_t _set_ec(cmdObj_t *cmd);		// expand CRLF on TX outout
static uint8_t _set_ee(cmdObj_t *cmd);		// enable character echo
static uint8_t _set_ex(cmdObj_t *cmd);		// enable XON/XOFF
static uint8_t _set_baud(cmdObj_t *cmd);	// set USB baud rate
*/

/***** PROGMEM Strings ******************************************************/

/* strings used by formatted print functions */
/*
static const char msg_units0[] PROGMEM = " in";	// used by generic print functions
static const char msg_units1[] PROGMEM = " mm";
static const char msg_units2[] PROGMEM = " deg";
static PGM_P const msg_units[] PROGMEM = { msg_units0, msg_units1, msg_units2 };
#define F_DEG 2
*/

/* PROGMEM strings for print formatting
 * NOTE: DO NOT USE TABS IN FORMAT STRINGS
 */
static const char fmt_nul[] PROGMEM = "";
static const char fmt_ui8[] PROGMEM = "%d\n";	// generic format for ui8s
static const char fmt_dbl[] PROGMEM = "%f\n";	// generic format for doubles
static const char fmt_str[] PROGMEM = "%s\n";	// generic format for string message (with no formatting)

// System group and ungrouped formatting strings
static const char fmt_fv[] PROGMEM = "[fv]  firmware version%16.2f\n";
static const char fmt_fb[] PROGMEM = "[fb]  firmware build%18.2f\n";
static const char fmt_hv[] PROGMEM = "[hv]  hardware version%16.2f\n";
static const char fmt_id[] PROGMEM = "[id]  TinyG ID%30s\n";

/***** PROGMEM config array **************************************************
 *
 *	NOTES:
 *	- Token matching occurs from the most specific to the least specific.
 *	  This means that if shorter tokens overlap longer ones the longer one
 *	  must precede the shorter one. E.g. "gco" needs to come before "gc"
 *
 *	- Mark group strings for entries that have no group as nul -->" ". 
 *	  This is important for group expansion.
 *
 *	- Groups do not have groups. Neither do uber-groups, e.g.
 *	  'x' is --> { "", "x",  	and 'm' is --> { "", "m",  
 */

const cfgItem_t cfgArray[] PROGMEM = {
	// grp  token flags format*, print_func, get_func, set_func  target for get/set,   default value
	{ "sys","fb", _f07, fmt_fb, _print_dbl, _get_dbl, _set_nul, (double *)&cfg.fw_build,   BUILD_NUMBER }, // MUST BE FIRST!
	{ "sys","fv", _f07, fmt_fv, _print_dbl, _get_dbl, _set_nul, (double *)&cfg.fw_version, VERSION_NUMBER },
	{ "sys","hv", _f07, fmt_hv, _print_dbl, _get_dbl, _set_hv,  (double *)&cfg.hw_version, HARDWARE_VERSION },
//	{ "sys","id", _fns, fmt_id, _print_str, _get_id,  _set_nul, (double *)&kc.null, 0 },		// device ID (ASCII signature)
/*
	// Reports, tests, help, and messages
	{ "", "sr",  _f00, fmt_nul, _print_sr,  _get_sr,  _set_sr,  (double *)&kc.null, 0 },		// status report object
	{ "", "qr",  _f00, fmt_qr,  _print_int, _get_qr,  _set_nul, (double *)&kc.null, 0 },		// queue report setting
	{ "", "rx",  _f00, fmt_rx,  _print_int, _get_rx,  _set_nul, (double *)&kc.null, 0 },		// space in RX buffer
	{ "", "msg", _f00, fmt_str, _print_str, _get_nul, _set_nul, (double *)&kc.null, 0 },		// string for generic messages
	{ "", "test",_f00, fmt_nul, _print_nul, print_test_help, tg_test, (double *)&kc.test,0 },	// prints test help screen
	{ "", "defa",_f00, fmt_nul, _print_nul, print_defaults_help,_set_defa,(double *)&kc.null,0},// prints defaults help screen
	{ "", "boot",_f00, fmt_nul, _print_nul, print_boot_loader_help,_set_nul, (double *)&kc.null,0 },
	{ "", "help",_f00, fmt_nul, _print_nul, print_config_help,_set_nul, (double *)&kc.null,0 },	// prints config help screen
	{ "", "h",   _f00, fmt_nul, _print_nul, print_config_help,_set_nul, (double *)&kc.null,0 },	// alias for "help"
*/
	// System parameters
	// NOTE: The ordering within the gcode defaults is important for token resolution
	// NOTE: Some values have been removed from the system group but are still accessible as individual elements
//	{ "sys","ic",  _f07, fmt_ic, _print_ui8, _get_ui8, _set_ic,  (double *)&cfg.ignore_crlf,		COM_IGNORE_CRLF },
//	{ "sys","ec",  _f07, fmt_ec, _print_ui8, _get_ui8, _set_ec,  (double *)&cfg.enable_cr,			COM_EXPAND_CR },
//	{ "sys","ee",  _f07, fmt_ee, _print_ui8, _get_ui8, _set_ee,  (double *)&cfg.enable_echo,		COM_ENABLE_ECHO },
//	{ "sys","ex",  _f07, fmt_ex, _print_ui8, _get_ui8, _set_ex,  (double *)&cfg.enable_xon,			COM_ENABLE_XON },
//	{ "sys","eq",  _f07, fmt_eq, _print_ui8, _get_ui8, _set_ui8, (double *)&cfg.enable_qr,			QR_VERBOSITY },
//	{ "sys","ej",  _f07, fmt_ej, _print_ui8, _get_ui8, _set_ui8, (double *)&cfg.comm_mode,			COMM_MODE },
//	{ "sys","jv",  _f07, fmt_jv, _print_ui8, _get_ui8, cmd_set_jv,(double *)&cfg.json_verbosity,	JSON_VERBOSITY },
//	{ "sys","tv",  _f07, fmt_tv, _print_ui8, _get_ui8, cmd_set_tv,(double *)&cfg.text_verbosity,	TEXT_VERBOSITY },
//	{ "sys","si",  _f07, fmt_si, _print_dbl, _get_int, _set_si,  (double *)&cfg.status_report_interval,STATUS_REPORT_INTERVAL_MS },
//	{ "sys","sv",  _f07, fmt_sv, _print_ui8, _get_ui8, _set_ui8, (double *)&cfg.status_report_verbosity,SR_VERBOSITY },
//	{ "sys","baud",_fns, fmt_baud,_print_ui8,_get_ui8, _set_baud,(double *)&cfg.usb_baud_rate,		XIO_BAUD_115200 },

/*	// Persistence for status report - must be in sequence
	// *** Count must agree with CMD_STATUS_REPORT_LEN in config.h ***
	{ "","se00",_fpe, fmt_nul, _print_nul, _get_int, _set_int,(double *)&cfg.status_report_list[0],0 },
	{ "","se01",_fpe, fmt_nul, _print_nul, _get_int, _set_int,(double *)&cfg.status_report_list[1],0 },
	{ "","se02",_fpe, fmt_nul, _print_nul, _get_int, _set_int,(double *)&cfg.status_report_list[2],0 },
	{ "","se03",_fpe, fmt_nul, _print_nul, _get_int, _set_int,(double *)&cfg.status_report_list[3],0 },
*/
	// Group lookups - must follow the single-valued entries for proper sub-string matching
	// *** Must agree with CMD_COUNT_GROUPS below ****
	{ "","sys",_f00, fmt_nul, _print_nul, _get_grp, _set_grp,(double *)&kc.null,0 },	// system group

	// Uber-group (groups of groups, for text-mode displays only)
	// *** Must agree with CMD_COUNT_UBER_GROUPS below ****
	{ "", "$", _f00, fmt_nul, _print_nul, _get_nul, _set_nul,(double *)&kc.null,0 }
};

#define CMD_COUNT_GROUPS 		1		// count of simple groups
#define CMD_COUNT_UBER_GROUPS 	1 		// count of uber-groups

#define CMD_INDEX_MAX (sizeof cfgArray / sizeof(cfgItem_t))
#define CMD_INDEX_END_SINGLES		(CMD_INDEX_MAX - CMD_COUNT_UBER_GROUPS - CMD_COUNT_GROUPS - CMD_STATUS_REPORT_LEN)
#define CMD_INDEX_START_GROUPS		(CMD_INDEX_MAX - CMD_COUNT_UBER_GROUPS - CMD_COUNT_GROUPS)
#define CMD_INDEX_START_UBER_GROUPS (CMD_INDEX_MAX - CMD_COUNT_UBER_GROUPS)

#define _index_is_single(i) ((i <= CMD_INDEX_END_SINGLES) ? true : false)	// Evaluators
#define _index_lt_groups(i) ((i <= CMD_INDEX_START_GROUPS) ? true : false)
#define _index_is_group(i) (((i >= CMD_INDEX_START_GROUPS) && (i < CMD_INDEX_START_UBER_GROUPS)) ? true : false)
#define _index_is_uber(i)   ((i >= CMD_INDEX_START_UBER_GROUPS) ? true : false)
#define _index_is_group_or_uber(i) ((i >= CMD_INDEX_START_GROUPS) ? true : false)

//index_t cmd_get_max_index() { return (CMD_INDEX_MAX);}
uint8_t cmd_index_is_group(index_t index) { return _index_is_group(index);}

/**** Versions, IDs, and simple reports  ****
 * _set_hv() - set hardweare version number
 * _get_id() - get device ID (signature)
 * _get_qr() - run queue report
 * _get_rx() - get bytes available in RX buffer
 */
static uint8_t _set_hv(cmdObj_t *cmd) 
{
	_set_dbl(cmd);					// record the hardware version
//	sys_port_bindings(cmd->value);	// reset port bindings
//	gpio_init();					// re-initialize the GPIO ports
	return (SC_OK);
}

/**** REPORT FUNCTIONS ****
 * _get_sr()   - run status report
 * _print_sr() - run status report
 * _set_sr()   - set status report specification
 * _set_si()   - set status report interval
 * cmd_set_jv() - set JSON verbosity level (exposed) - for details see jsonVerbosity in config.h
 * cmd_set_tv() - set text verbosity level (exposed) - for details see textVerbosity in config.h
 */
static uint8_t _get_sr(cmdObj_t *cmd)
{
//	rpt_populate_unfiltered_status_report();
	return (SC_OK);
}

static void _print_sr(cmdObj_t *cmd)
{
//	rpt_populate_unfiltered_status_report();
}

static uint8_t _set_sr(cmdObj_t *cmd)
{
/*
	memset(cfg.status_report_list, 0 , sizeof(cfg.status_report_list));
	for (uint8_t i=0; i<CMD_STATUS_REPORT_LEN; i++) {
		if ((cmd = cmd->nx) == NULL) break;
		cfg.status_report_list[i] = cmd->index;
		cmd->value = cmd->index;	// you want to persist the index as the value
		cmd_persist(cmd);
	}
*/
	return (SC_OK);
}

static uint8_t _set_si(cmdObj_t *cmd) 
{
/*
	if ((cmd->value < STATUS_REPORT_MIN_MS) && (cmd->value!=0)) {
		cmd->value = STATUS_REPORT_MIN_MS;
	}
	cfg.status_report_interval = (uint32_t)cmd->value;
*/
	return(SC_OK);
}

uint8_t cmd_set_jv(cmdObj_t *cmd) 
{
/*
	cfg.json_verbosity = cmd->value;

	cfg.echo_json_footer = false;
	cfg.echo_json_configs = false;
	cfg.echo_json_messages = false;
	cfg.echo_json_linenum = false;
	cfg.echo_json_gcode_block = false;

	if (cmd->value >= JV_FOOTER) 	{ cfg.echo_json_footer = true;}
	if (cmd->value >= JV_CONFIGS)	{ cfg.echo_json_configs = true;}
	if (cmd->value >= JV_MESSAGES)	{ cfg.echo_json_messages = true;}
	if (cmd->value >= JV_LINENUM)	{ cfg.echo_json_linenum = true;}
	if (cmd->value >= JV_VERBOSE)	{ cfg.echo_json_gcode_block = true;}
*/
	return(SC_OK);
}

uint8_t cmd_set_tv(cmdObj_t *cmd) 
{
/*
	cfg.text_verbosity = cmd->value;

	cfg.echo_text_prompt = false;
	cfg.echo_text_messages = false;
	cfg.echo_text_configs = false;
	cfg.echo_text_gcode_block = false;

	if (cmd->value >= TV_PROMPT)	{ cfg.echo_text_prompt = true;} 
	if (cmd->value >= TV_MESSAGES)	{ cfg.echo_text_messages = true;}
	if (cmd->value >= TV_CONFIGS)	{ cfg.echo_text_configs = true;}
	if (cmd->value >= TV_VERBOSE)	{ cfg.echo_text_gcode_block = true;}
*/
	return(SC_OK);
}

/**** COMMUNICATIONS SETTINGS ****
 * _set_ic() - ignore CR or LF on RX
 * _set_ec() - enable CRLF on TX
 * _set_ee() - enable character echo
 * _set_ex() - enable XON/XOFF
 * _set_baud() - set USB baud rate
 *	The above assume USB is the std device
 */

static uint8_t _set_comm_helper(cmdObj_t *cmd, uint32_t yes, uint32_t no)
{
/*
	if (fp_NOT_ZERO(cmd->value)) { 
		(void)xio_ctrl(XIO_DEV_USB, yes);
	} else { 
		(void)xio_ctrl(XIO_DEV_USB, no);
	}
*/
	return (SC_OK);
}

static uint8_t _set_ic(cmdObj_t *cmd) 	// ignore CR or LF on RX
{
/*	cfg.ignore_crlf = (uint8_t)cmd->value;
	(void)xio_ctrl(XIO_DEV_USB, XIO_NOIGNORECR);	// clear them both
	(void)xio_ctrl(XIO_DEV_USB, XIO_NOIGNORELF);

	if (cfg.ignore_crlf == IGNORE_CR) {				// $ic=1
		(void)xio_ctrl(XIO_DEV_USB, XIO_IGNORECR);
	} else if (cfg.ignore_crlf == IGNORE_LF) {		// $ic=2
		(void)xio_ctrl(XIO_DEV_USB, XIO_IGNORELF);
	}
*/
	return (SC_OK);
}

static uint8_t _set_ec(cmdObj_t *cmd) 	// expand CR to CRLF on TX
{
/*	cfg.enable_cr = (uint8_t)cmd->value;
	return(_set_comm_helper(cmd, XIO_CRLF, XIO_NOCRLF));
*/
	return (SC_OK);
}

static uint8_t _set_ee(cmdObj_t *cmd) 	// enable character echo
{
/*	cfg.enable_echo = (uint8_t)cmd->value;
	return(_set_comm_helper(cmd, XIO_ECHO, XIO_NOECHO));
*/
	return (SC_OK);
}

static uint8_t _set_ex(cmdObj_t *cmd)		// enable XON/XOFF
{
/*	cfg.enable_xon = (uint8_t)cmd->value;
	return(_set_comm_helper(cmd, XIO_XOFF, XIO_NOXOFF));
*/
	return (SC_OK);
}

/*
 * _set_baud() - set USB baud rate
 *
 *	See xio_usart.h for valid values. Works as a callback.
 *	The initial routine changes the baud config setting and sets a flag
 *	Then it posts a user message indicating the new baud rate
 *	Then it waits for the TX buffer to empty (so the message is sent)
 *	Then it performs the callback to apply the new baud rate
 */

static uint8_t _set_baud(cmdObj_t *cmd)
{
/*	uint8_t baud = (uint8_t)cmd->value;
	if ((baud < 1) || (baud > 6)) {
		cmd_add_message_P(PSTR("*** WARNING *** Illegal baud rate specified"));
		return (SC_INPUT_VALUE_UNSUPPORTED);
	}
	cfg.usb_baud_rate = baud;
	cfg.usb_baud_flag = true;
	char message[CMD_MESSAGE_LEN]; 
	sprintf_P(message, PSTR("*** NOTICE *** Restting baud rate to %S"),(PGM_P)pgm_read_word(&msg_baud[baud]));
	cmd_add_message(message);
*/
	return (SC_OK);
}

uint8_t cfg_baud_rate_callback(void) 
{
/*	if (cfg.usb_baud_flag == false) { return(SC_NOOP);}
	cfg.usb_baud_flag = false;
	xio_set_baud(XIO_DEV_USB, cfg.usb_baud_rate);
*/
	return (SC_OK);
}

/**** UberGroup Operations ****
 * Uber groups are groups of groups organized for convenience:
 *	- all		- group of all groups
 * _do_all()		- get and print all groups uber group
 */
/*
static void _do_group_list(cmdObj_t *cmd, char list[][CMD_TOKEN_LEN+1]) // helper to print multiple groups in a list
{
	for (uint8_t i=0; i < CMD_MAX_OBJECTS; i++) {
		if (list[i][0] == NUL) return;
		cmd = cmd_body;
		strncpy(cmd->token, list[i], CMD_TOKEN_LEN);
		cmd->index = cmd_get_index("", cmd->token);
//		cmd->type = TYPE_PARENT;
		cmd_get_cmdObj(cmd);
		cmd_print_list(SC_OK, TEXT_MULTILINE_FORMATTED, JSON_RESPONSE_FORMAT);
	}
}


static uint8_t _do_all(cmdObj_t *cmd)		// print all parameters
{
	// print system group
	strcpy(cmd->token,"sys");
	_get_grp(cmd);
	cmd_print_list(SC_OK, TEXT_MULTILINE_FORMATTED,  JSON_RESPONSE_FORMAT);

	_do_offsets(cmd);
	_do_motors(cmd);
	_do_axes(cmd);

	// print PWM group
	strcpy(cmd->token,"p1");
	_get_grp(cmd);
	cmd_print_list(SC_OK, TEXT_MULTILINE_FORMATTED, JSON_RESPONSE_FORMAT);

	return (SC_COMPLETE);
}
*/
/*****************************************************************************
 *****************************************************************************
 *****************************************************************************
 *** END SETTING-SPECIFIC REGION *********************************************
 *** Code below should not require changes as parameters are added/updated ***
 *****************************************************************************
 *****************************************************************************
 *****************************************************************************/

/****************************************************************************/
/**** CMD FUNCTION ENTRY POINTS *********************************************/
/****************************************************************************/
/* These are the primary access points to cmd functions
 * These are the gatekeeper functions that check index ranges so others don't have to
 *
 * cmd_set() 	- Write a value or invoke a function - operates on single valued elements or groups
 * cmd_get() 	- Build a cmdObj with the values from the target & return the value
 *			   	  Populate cmd body with single valued elements or groups (iterates)
 * cmd_print()	- Output a formatted string for the value.
 * cmd_persist()- persist value to NVM. Takes special cases into account
 */

#define ASSERT_CMD_INDEX(a) if (cmd->index >= CMD_INDEX_MAX) return (a);

uint8_t cmd_set(cmdObj_t *cmd)
{
	ASSERT_CMD_INDEX(SC_UNRECOGNIZED_COMMAND);
	return (((fptrCmd)(pgm_read_word(&cfgArray[cmd->index].set)))(cmd));
}

uint8_t cmd_get(cmdObj_t *cmd)
{
	ASSERT_CMD_INDEX(SC_UNRECOGNIZED_COMMAND);
	return (((fptrCmd)(pgm_read_word(&cfgArray[cmd->index].get)))(cmd));
}

void cmd_print(cmdObj_t *cmd)
{
	if (cmd->index >= CMD_INDEX_MAX) return;
	((fptrPrint)(pgm_read_word(&cfgArray[cmd->index].print)))(cmd);
}

void cmd_persist(cmdObj_t *cmd)
{
#ifdef __DISABLE_PERSISTENCE	// cutout for faster simulation in test
	return;
#endif
//	if (_index_lt_groups(cmd->index) == false) return;
//	if (pgm_read_byte(&cfgArray[cmd->index].flags) & F_PERSIST) {
//		cmd_write_NVM_value(cmd);
//	}
}

/****************************************************************************
 * cfg_init() - called once on hard reset
 * _set_defa() - reset NVM with default values for active profile
 *
 * Will perform one of 2 actions:
 *	(1) if NVM is set up or out-of-rev: load RAM and NVM with hardwired default settings
 *	(2) if NVM is set up and at current config version: use NVM data for config
 *
 *	You can assume the cfg struct has been zeroed by a hard reset. 
 *	Do not clear it as the version and build numbers have already been set by tg_init()
 */
void cfg_init()
{
	cmdObj_t *cmd = cmd_reset_list();
//	cm_set_units_mode(MILLIMETERS);			// must do init in MM mode
	cfg.comm_mode = JSON_MODE;				// initial value until EEPROM is read
//	cfg.nvm_base_addr = NVM_BASE_ADDR;
//	cfg.nvm_profile_base = cfg.nvm_base_addr;
	cmd->index = 0;							// this will read the first record in NVM
//	cmd_read_NVM_value(cmd);
/*
	// Case (1) NVM is not setup or not in revision
	if (cmd->value != cfg.fw_build) {
		cmd->value = true;
		_set_defa(cmd);		// this subroutine called from here and from the $defa=1 command

	// Case (2) NVM is setup and in revision
	} else {
		tg_print_loading_configs_message();
		for (cmd->index=0; _index_is_single(cmd->index); cmd->index++) {
			if (pgm_read_byte(&cfgArray[cmd->index].flags) & F_INITIALIZE) {
				strcpy_P(cmd->token, cfgArray[cmd->index].token);	// read the token from the array
				cmd_read_NVM_value(cmd);
				cmd_set(cmd);
			}
		}
	}
	rpt_init_status_report(true);// requires special treatment (persist = true)
*/
}

static uint8_t _set_defa(cmdObj_t *cmd) 
{
/*
	if (cmd->value != true) {		// failsafe. Must set true or no action occurs
		print_defaults_help(cmd);
		return (SC_OK);
	}
	cm_set_units_mode(MILLIMETERS);	// must do init in MM mode
	tg_print_initializing_message();

	for (cmd->index=0; _index_is_single(cmd->index); cmd->index++) {
		if (pgm_read_byte(&cfgArray[cmd->index].flags) & F_INITIALIZE) {
			cmd->value = (double)pgm_read_float(&cfgArray[cmd->index].def_value);
			strcpy_P(cmd->token, cfgArray[cmd->index].token);
			cmd_set(cmd);
			cmd_persist(cmd);
		}
	}
*/
	return (SC_OK);
}

/****************************************************************************
 * cfg_text_parser() - update a config setting from a text block (text mode)
 * _text_parser() 	 - helper for above
 * 
 * Use cases handled:
 *	- $xfr=1200	set a parameter
 *	- $xfr		display a parameter
 *	- $x		display a group
 *	- ?			generate a status report (multiline format)
 */
uint8_t cfg_text_parser(char *str)
{
/*
	cmdObj_t *cmd = cmd_reset_list();		// returns first object in the body
	uint8_t status = SC_OK;

	if (str[0] == '?') {					// handle status report case
		rpt_run_text_status_report();
		return (SC_OK);
	}
	if ((str[0] == '$') && (str[1] == NUL)) {  // treat a lone $ as a sys request
		strcat(str,"sys");
	}
	// single-unit parser processing
	ritorno(_text_parser(str, cmd));		// decode the request or return if error
	if ((cmd->type == TYPE_PARENT) || (cmd->type == TYPE_NULL)) {
		if (cmd_get(cmd) == SC_COMPLETE) {	// populate value, group values, or run uber-group displays
			return (SC_OK);					// return for uber-group displays so they don't print twice
		}
	} else { 								// process SET and RUN commands
		status = cmd_set(cmd);				// set single value
		cmd_persist(cmd);
	}
	cmd_print_list(status, TEXT_MULTILINE_FORMATTED, JSON_RESPONSE_FORMAT); // print the results
	return (status);
*/
	return (SC_OK);
}
/*
static uint8_t _text_parser(char *str, cmdObj_t *cmd)
{
	char *ptr_rd, *ptr_wr;					// read and write pointers
	char separators[] = {" =:|\t"};			// any separator someone might use

	// string pre-processing
	cmd_reset_obj(cmd);						// initialize config object
	if (*str == '$') str++;					// ignore leading $
	for (ptr_rd = ptr_wr = str; *ptr_rd!=NUL; ptr_rd++, ptr_wr++) {
		*ptr_wr = tolower(*ptr_rd);			// convert string to lower case
		if (*ptr_rd==',') {
			*ptr_wr = *(++ptr_rd);			// skip over comma
		}
	}
	*ptr_wr = NUL;

	// field processing
	cmd->type = TYPE_NULL;
	if ((ptr_rd = strpbrk(str, separators)) == NULL) { // no value part
		strncpy(cmd->token, str, CMD_TOKEN_LEN);
	} else {
		*ptr_rd = NUL;						// terminate at end of name
		strncpy(cmd->token, str, CMD_TOKEN_LEN);
		str = ++ptr_rd;
		cmd->value = strtod(str, &ptr_rd);	// ptr_rd used as end pointer
		if (ptr_rd != str) {
			cmd->type = TYPE_FLOAT;
		}
	}
	if ((cmd->index = cmd_get_index("",cmd->token)) == NO_MATCH) { 
		return (SC_UNRECOGNIZED_COMMAND);
	}
	return (SC_OK);
}
*/

/***** Generic Internal Functions *******************************************
 * Generic sets()
 * _set_nul() - set nothing (returns SC_NOOP)
 * _set_ui8() - set value as 8 bit uint8_t value w/o unit conversion
 * _set_int() - set value as 32 bit integer w/o unit conversion
 * _set_dbl() - set value as double w/o unit conversion
 *
 * Generic gets()
 * _get_nul() - get nothing (returns SC_NOOP)
 * _get_ui8() - get value as 8 bit uint8_t w/o unit conversion
 * _get_int() - get value as 32 bit integer w/o unit conversion
 * _get_dbl() - get value as double w/o unit conversion
 */
static uint8_t _set_nul(cmdObj_t *cmd) { return (SC_NOOP);}

static uint8_t _set_ui8(cmdObj_t *cmd)
{
	*((uint8_t *)pgm_read_word(&cfgArray[cmd->index].target)) = cmd->value;
	cmd->type = TYPE_INTEGER;
	return(SC_OK);
}

static uint8_t _set_int(cmdObj_t *cmd)
{
	*((uint32_t *)pgm_read_word(&cfgArray[cmd->index].target)) = cmd->value;
	cmd->type = TYPE_INTEGER;
	return(SC_OK);
}

static uint8_t _set_dbl(cmdObj_t *cmd)
{
	*((double *)pgm_read_word(&cfgArray[cmd->index].target)) = cmd->value;
	cmd->type = TYPE_FLOAT;
	return(SC_OK);
}

static uint8_t _get_nul(cmdObj_t *cmd) 
{ 
	cmd->type = TYPE_NULL;
	return (SC_NOOP);
}

static uint8_t _get_ui8(cmdObj_t *cmd)
{
	cmd->value = (double)*((uint8_t *)pgm_read_word(&cfgArray[cmd->index].target));
	cmd->type = TYPE_INTEGER;
	return (SC_OK);
}

static uint8_t _get_int(cmdObj_t *cmd)
{
	cmd->value = (double)*((uint32_t *)pgm_read_word(&cfgArray[cmd->index].target));
	cmd->type = TYPE_INTEGER;
	return (SC_OK);
}

static uint8_t _get_dbl(cmdObj_t *cmd)
{
	cmd->value = *((double *)pgm_read_word(&cfgArray[cmd->index].target));
	cmd->type = TYPE_FLOAT;
	return (SC_OK);
}

/* Generic prints()
 * _print_nul() - print nothing
 * _print_str() - print string value
 * _print_ui8() - print uint8_t value w/no units or unit conversion
 * _print_int() - print integer value w/no units or unit conversion
 * _print_dbl() - print double value w/no units or unit conversion
 */
static void _print_nul(cmdObj_t *cmd) {}

static void _print_str(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), *cmd->stringp);
}

static void _print_ui8(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), (uint8_t)cmd->value);
}

static void _print_int(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), (uint32_t)cmd->value);
}

static void _print_dbl(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), cmd->value);
}


/***************************************************************************** 
 * Accessors - get various data from an object given the index
 * _get_format() 	- return format string for an index
 * _get_motor()		- return the motor number as an index or -1 if na
 * _get_axis()		- return the axis as an index or -1 if na 
 * _get_pos_axis()	- return axis number for pos values or -1 if none - e.g. posx
 */
static char *_get_format(const index_t i, char *format)
{
	strncpy_P(format, (PGM_P)pgm_read_word(&cfgArray[i].format), CMD_FORMAT_LEN);
	return (format);
}
/*
static int8_t _get_motor(const index_t i)
{
	char *ptr;
	char motors[] = {"1234"};
	char tmp[CMD_TOKEN_LEN+1];

	strcpy_P(tmp, cfgArray[i].group);
	if ((ptr = strchr(motors, tmp[0])) == NULL) {
		return (-1);
	}
	return (ptr - motors);
}

static int8_t _get_axis(const index_t i)
{
	char *ptr;
	char tmp[CMD_TOKEN_LEN+1];
	char axes[] = {"xyzabc"};

	strcpy_P(tmp, cfgArray[i].token);
	if ((ptr = strchr(axes, tmp[0])) == NULL) { return (-1);}
	return (ptr - axes);
}

static int8_t _get_pos_axis(const index_t i)
{
	char *ptr;
	char tmp[CMD_TOKEN_LEN+1];
	char axes[] = {"xyzabc"};

	strcpy_P(tmp, cfgArray[i].token);
	if ((ptr = strchr(axes, tmp[3])) == NULL) { return (-1);}
	return (ptr - axes);
}
*/

/********************************************************************************
 * Group operations
 *
 *	Group operations work on parent/child groups where the parent is one of:
 *	  axis group 			x,y,z,a,b,c
 *	  motor group			1,2,3,4
 *	  PWM group				p1
 *	  coordinate group		g54,g55,g56,g57,g58,g59,g92
 *	  system group			"sys" - a collection of otherwise unrelated variables
 *
 *	Text mode can only GET groups. For example:
 *	  $x					get all members of an axis group
 *	  $1					get all members of a motor group
 *	  $<grp>				get any named group from the above lists
 *
 *	In JSON groups are carried as parent / child objects & can get and set elements:
 *	  {"x":""}						get all X axis parameters
 *	  {"x":{"vm":""}}				get X axis velocity max 
 *	  {"x":{"vm":1000}}				set X axis velocity max
 *	  {"x":{"vm":"","fr":""}}		get X axis velocity max and feed rate 
 *	  {"x":{"vm":1000,"fr";900}}	set X axis velocity max and feed rate
 *	  {"x":{"am":1,"fr":800,....}}	set multiple or all X axis parameters
 */

/*
 * _get_grp() - read data from axis, motor, system or other group
 *
 *	_get_grp() is a group expansion function that expands the parent group and 
 *	returns the values of all the children in that group. It expects the first 
 *	cmdObj in the cmdBody to have a valid group name in the token field. This 
 *	first object will be set to a TYPE_PARENT. The group field is left nul -  
 *	as the group field refers to a parent group, which this group has none.
 *
 *	All subsequent cmdObjs in the body will be populated with their values.
 *	The token field will be populated as will the parent name in the group field. 
 *
 *	The sys group is an exception where the childern carry a blank group field, 
 *	even though the sys parent is labeled as a TYPE_PARENT.
 */

static uint8_t _get_grp(cmdObj_t *cmd)
{
	char *parent_group = cmd->token;		// token in the parent cmd object is the group
	char group[CMD_GROUP_LEN+1];			// group string retrieved from cfgArray child
	cmd->type = TYPE_PARENT;				// make first object the parent 
	for (index_t i=0; i<=CMD_INDEX_END_SINGLES; i++) {
		strcpy_P(group, cfgArray[i].group);  // don't need strncpy as it's always terminated
		if (strcmp(parent_group, group) != 0) continue;
		(++cmd)->index = i;
		cmd_get_cmdObj(cmd);
	}
	return (SC_OK);
}

/*
 * _set_grp() - get or set one or more values in a group
 *
 *	This functions is called "_set_group()" but technically it's a getter and 
 *	a setter. It iterates the group children and either gets the value or sets
 *	the value for each depending on the cmd->type.
 *
 *	This function serves JSON mode only as text mode shouldn't call it.
 */

static uint8_t _set_grp(cmdObj_t *cmd)
{
	if (cfg.comm_mode == TEXT_MODE) return (SC_UNRECOGNIZED_COMMAND);
	for (uint8_t i=0; i<CMD_MAX_OBJECTS; i++) {
		if ((cmd = cmd->nx) == NULL) break;
		if (cmd->type == TYPE_EMPTY) break;
		else if (cmd->type == TYPE_NULL)	// NULL means GET the value
			cmd_get(cmd);
		else {
			cmd_set(cmd);
			cmd_persist(cmd);
		}
	}
	return (SC_OK);
}

/*
 * cmd_group_is_prefixed() - hack
 *
 *	This little function deals with the fact that some groups don't use the parent 
 *	token as a prefix to the child elements; SR being a good example.
 */
uint8_t cmd_group_is_prefixed(char *group)
{
	if (strstr("sr",group) != NULL) {	// you can extend like this: "sr,sys,xyzzy"
		return (false);
	}
	return (true);
}

/*****************************************************************************
 ***** cmdObj functions ******************************************************
 *****************************************************************************/

/*****************************************************************************
 * cmdObj helper functions and other low-level cmd helpers
 * cmd_get_index() 		 - get index from mnenonic token + group
 * cmd_get_type()		 - returns command type as a CMD_TYPE enum
 * cmd_persist_offsets() - write any changed G54 (et al) offsets back to NVM
 */

/* 
 * cmd_get_index() is the most expensive routine in the whole config. It does a linear table scan 
 * of the PROGMEM strings, which of course could be further optimized with indexes or hashing.
 */
index_t cmd_get_index(const char *group, const char *token)
{
	char c;
	char str[CMD_TOKEN_LEN+1];
	strcpy(str, group);
	strcat(str, token);

	for (index_t i=0; i<CMD_INDEX_MAX; i++) {
		if ((c = (char)pgm_read_byte(&cfgArray[i].token[0])) != str[0]) {	// 1st character mismatch 
			continue;
		}
		if ((c = (char)pgm_read_byte(&cfgArray[i].token[1])) == NUL) {
			if (str[1] == NUL) return(i);									// one character match
		}
		if (c != str[1]) continue;											// 2nd character mismatch
		if ((c = (char)pgm_read_byte(&cfgArray[i].token[2])) == NUL) {
			if (str[2] == NUL) return(i);									// two character match
		}
		if (c != str[2]) continue;											// 3rd character mismatch
		if ((c = (char)pgm_read_byte(&cfgArray[i].token[3])) == NUL) {
			if (str[3] == NUL) return(i);									// three character match
		}
		if (c != str[3]) continue;											// 4th character mismatch
		if ((c = (char)pgm_read_byte(&cfgArray[i].token[4])) == NUL) {
			if (str[4] == NUL) return(i);									// four character match
		}
		if (c != str[4]) continue;											// 5th character mismatch
		return (i);															// five character match
	}
	return (NO_MATCH);
}
/*
uint8_t cmd_get_type(cmdObj_t *cmd)
{
	if (strcmp("gc", cmd->token) == 0) return (CMD_TYPE_GCODE);
	if (strcmp("sr", cmd->token) == 0) return (CMD_TYPE_REPORT);
	if (strcmp("qr", cmd->token) == 0) return (CMD_TYPE_REPORT);
	return (CMD_TYPE_CONFIG);
}
*/
/*
uint8_t cmd_persist_offsets(uint8_t flag)
{
	if (flag == true) {
		cmdObj_t cmd;
		for (uint8_t i=1; i<=COORDS; i++) {
			for (uint8_t j=0; j<AXES; j++) {
				sprintf(cmd.token, "g%2d%c", 53+i, ("xyzabc")[j]);
				cmd.index = cmd_get_index("", cmd.token);
				cmd.value = cfg.offset[i][j];
				cmd_persist(&cmd);				// only writes changed values
			}
		}
	}
	return (SC_OK);
}
*/
/*
 * cmdObj low-level object and list operations
 * cmd_get_cmdObj()		- setup a cmd object by providing the index
 * cmd_reset_obj()		- quick clear for a new cmd object
 * cmd_reset_list()		- clear entire header, body and footer for a new use
 * cmd_copy_string()	- used to write a string to shared string storage and link it
 * cmd_copy_string_P()	- same, but for progmem string sources
 * cmd_add_object()		- write contents of parameter to  first free object in the body
 * cmd_add_integer()	- add an integer value to end of cmd body (Note 1)
 * cmd_add_float()		- add a floating point value to end of cmd body
 * cmd_add_string()		- add a string object to end of cmd body
 * cmd_add_string_P()	- add a program memory string as a string object to end of cmd body
 * cmd_add_message()	- add a mesasge to cmd body
 * cmd_add_message_P()	- add a program memory message the the cmd body
 *
 *	Note: Functions that return a cmd pointer point to the object that was modified
 *	or a NULL pointer if there was an error
 *
 *	Note Adding a really large integer (like a checksum value) may lose precision 
 *	due to the cast to a double. Sometimes it's better to load an integer as a 
 *	string if all you want to do is display it.
 */

void cmd_get_cmdObj(cmdObj_t *cmd)
{
	if (cmd->index >= CMD_INDEX_MAX) return;
	index_t tmp = cmd->index;
	cmd_reset_obj(cmd);
	cmd->index = tmp;

	strcpy_P(cmd->group, cfgArray[cmd->index].group); // group field is always terminated
	strcpy_P(cmd->token, cfgArray[cmd->index].token); // token field is always terminated

	// special processing for system groups and stripping tokens for groups
	if (cmd->group[0] != NUL) {
		if (pgm_read_byte(&cfgArray[cmd->index].flags) & F_NOSTRIP) {
			cmd->group[0] = NUL;
		} else {
			strcpy(cmd->token, &cmd->token[strlen(cmd->group)]); // strip group from the token
		}
	}
	((fptrCmd)(pgm_read_word(&cfgArray[cmd->index].get)))(cmd);	// populate the value
}
 
cmdObj_t *cmd_reset_obj(cmdObj_t *cmd)	// clear a single cmdObj structure
{
	cmd->type = TYPE_EMPTY;				// selective clear is much faster than calling memset
	cmd->index = 0;
	cmd->value = 0;
	cmd->token[0] = NUL;
	cmd->group[0] = NUL;
	cmd->stringp = NULL;

	if (cmd->pv == NULL) { 				// set depth correctly
		cmd->depth = 0;
	} else {
		if (cmd->pv->type == TYPE_PARENT) { 
			cmd->depth = cmd->pv->depth + 1;
		} else {
			cmd->depth = cmd->pv->depth;
		}
	}
	return (cmd);
}

cmdObj_t *cmd_reset_list()					// clear the header and response body
{
	cmdStr.wp = 0;							// reset the shared string
	cmdObj_t *cmd = cmd_list;				// set up linked list and initialize elements	
	for (uint8_t i=0; i<CMD_LIST_LEN; i++, cmd++) {
		cmd->pv = (cmd-1);					// the ends are bogus & corrected later
		cmd->nx = (cmd+1);
		cmd->index = 0;
		cmd->depth = 1;						// header and footer are corrected later
		cmd->type = TYPE_EMPTY;
		cmd->token[0] = NUL;
	}
	(--cmd)->nx = NULL;
	cmd = cmd_list;							// setup response header element ('r')
	cmd->pv = NULL;
	cmd->depth = 0;
	cmd->type = TYPE_PARENT;
	strcpy(cmd->token, "r");
	return (cmd_body);						// this is a convenience for calling routines
}

uint8_t cmd_copy_string(cmdObj_t *cmd, const char *src)
{
	if ((cmdStr.wp + strlen(src)) > CMD_SHARED_STRING_LEN) { return (SC_BUFFER_FULL);}
	char *dst = &cmdStr.string[cmdStr.wp];
	strcpy(dst, src);						// copy string to current head position
	cmdStr.wp += strlen(src)+1;				// advance head for next string
	cmd->stringp = (char (*)[])dst;
	return (SC_OK);
}

uint8_t cmd_copy_string_P(cmdObj_t *cmd, const char *src_P)
{
	char buf[CMD_SHARED_STRING_LEN];
	strncpy_P(buf, src_P, CMD_SHARED_STRING_LEN);
	return (cmd_copy_string(cmd, buf));
}

cmdObj_t *cmd_add_object(char *token)		// add an object to the body using a token
{
	cmdObj_t *cmd = cmd_body;
	for (uint8_t i=0; i<CMD_BODY_LEN; i++) {
		if (cmd->type != TYPE_EMPTY) {
			cmd = cmd->nx;
			continue;
		}
		// load the index from the token or die trying
		if ((cmd->index = cmd_get_index("",token)) == NO_MATCH) { return (NULL);}
		cmd_get_cmdObj(cmd);				// populate the object from the index
		return (cmd);
	}
	return (NULL);
}

cmdObj_t *cmd_add_integer(char *token, const uint32_t value)// add an integer object to the body
{
	cmdObj_t *cmd = cmd_body;
	for (uint8_t i=0; i<CMD_BODY_LEN; i++) {
		if (cmd->type != TYPE_EMPTY) {
			cmd = cmd->nx;
			continue;
		}
		strncpy(cmd->token, token, CMD_TOKEN_LEN);
		cmd->value = (double) value;
		cmd->type = TYPE_INTEGER;
		return (cmd);
	}
	return (NULL);
}

cmdObj_t *cmd_add_float(char *token, const double value)	// add a float object to the body
{
	cmdObj_t *cmd = cmd_body;
	for (uint8_t i=0; i<CMD_BODY_LEN; i++) {
		if (cmd->type != TYPE_EMPTY) {
			cmd = cmd->nx;
			continue;
		}
		strncpy(cmd->token, token, CMD_TOKEN_LEN);
		cmd->value = value;
		cmd->type = TYPE_FLOAT;
		return (cmd);
	}
	return (NULL);
}

cmdObj_t *cmd_add_string(char *token, const char *string)	// add a string object to the body
{
	cmdObj_t *cmd = cmd_body;
	for (uint8_t i=0; i<CMD_BODY_LEN; i++) {
		if (cmd->type != TYPE_EMPTY) {
			cmd = cmd->nx;
			continue;
		}
		strncpy(cmd->token, token, CMD_TOKEN_LEN);
		if (cmd_copy_string(cmd, string) != SC_OK) { return (NULL);}
		cmd->index = cmd_get_index("", cmd->token);
		cmd->type = TYPE_STRING;
		return (cmd);
	}
	return (NULL);
}

cmdObj_t *cmd_add_string_P(char *token, const char *string)
{
	char message[CMD_MESSAGE_LEN]; 
	sprintf_P(message, string);
	return(cmd_add_string(token, message));
}

cmdObj_t *cmd_add_message(const char *string)	// conditionally add a message object to the body
{
	return(cmd_add_string("msg", string));
}

cmdObj_t *cmd_add_message_P(const char *string)	// conditionally add a message object to the body
{
	char message[CMD_MESSAGE_LEN]; 
	sprintf_P(message, string);
	return(cmd_add_string("msg", message));
}

/**** cmd_print_list() - print cmd_array as JSON or text ****
 *
 * 	Use this function for all text and JSON output that wants to be in a response header
 *	(don't just printf stuff)
 * 	It generates and prints the JSON and text mode output strings 
 *	In JSON mode it generates the footer with the status code, buffer count and checksum
 *	In text mode it uses the the textmode variable to set the output format
 *
 *	Inputs:
 *	  json_flags = JSON_OBJECT_FORMAT - print just the body w/o header or footer
 *	  json_flags = JSON_RESPONSE_FORMAT - print a full "r" object with footer
 *
 *	  text_flags = TEXT_INLINE_PAIRS - print text as name/value pairs on a single line
 *	  text_flags = TEXT_INLINE_VALUES - print text as comma separated values on a single line
 *	  text_flags = TEXT_MULTILINE_FORMATTED - print text one value per line with formatting string
 */
void cmd_print_list(uint8_t status, uint8_t text_flags, uint8_t json_flags)
{
	if (cfg.comm_mode == JSON_MODE) {
		switch (json_flags) {
			case JSON_NO_PRINT: { break; } 
			case JSON_OBJECT_FORMAT: { js_print_json_object(cmd_body); break; }
			case JSON_RESPONSE_FORMAT: { js_print_json_response(status); break; }
		}
	} else {
		switch (text_flags) {
			case TEXT_NO_PRINT: { break; } 
			case TEXT_INLINE_PAIRS: { _print_text_inline_pairs(); break; }
			case TEXT_INLINE_VALUES: { _print_text_inline_values(); break; }
			case TEXT_MULTILINE_FORMATTED: { _print_text_multiline_formatted();}
		}
	}
}

void _print_text_inline_pairs()
{
	cmdObj_t *cmd = cmd_body;

	for (uint8_t i=0; i<CMD_BODY_LEN-1; i++) {
		switch (cmd->type) {
			case TYPE_PARENT:	{ cmd = cmd->nx; continue; }
			case TYPE_FLOAT:	{ fprintf_P(stderr,PSTR("%s:%1.3f"), cmd->token, cmd->value); break;}
			case TYPE_INTEGER:	{ fprintf_P(stderr,PSTR("%s:%1.0f"), cmd->token, cmd->value); break;}
			case TYPE_STRING:	{ fprintf_P(stderr,PSTR("%s:%s"), cmd->token, *cmd->stringp); break;}
			case TYPE_EMPTY:	{ fprintf_P(stderr,PSTR("\n")); return; }
		}
		cmd = cmd->nx;
		if (cmd->type != TYPE_EMPTY) { fprintf_P(stderr,PSTR(","));}		
	}
}

void _print_text_inline_values()
{
	cmdObj_t *cmd = cmd_body;

	for (uint8_t i=0; i<CMD_BODY_LEN-1; i++) {
		switch (cmd->type) {
			case TYPE_PARENT:	{ cmd = cmd->nx; continue; }
			case TYPE_FLOAT:	{ fprintf_P(stderr,PSTR("%1.3f"), cmd->value); break;}
			case TYPE_INTEGER:	{ fprintf_P(stderr,PSTR("%1.0f"), cmd->value); break;}
			case TYPE_STRING:	{ fprintf_P(stderr,PSTR("%s"), *cmd->stringp); break;}
			case TYPE_EMPTY:	{ fprintf_P(stderr,PSTR("\n")); return; }
		}
		cmd = cmd->nx;
		if (cmd->type != TYPE_EMPTY) { fprintf_P(stderr,PSTR(","));}
	}
}

void _print_text_multiline_formatted()
{
	cmdObj_t *cmd = cmd_body;

	for (uint8_t i=0; i<CMD_BODY_LEN-1; i++) {
		if (cmd->type != TYPE_PARENT) { cmd_print(cmd);}
		cmd = cmd->nx;
		if (cmd->type == TYPE_EMPTY) { break;}
	}
}

/************************************************************************************
 ***** EEPROM access functions ******************************************************
 ************************************************************************************
 * cmd_read_NVM_value()	 - return value (as double) by index
 * cmd_write_NVM_value() - write to NVM by index, but only if the value has changed
 * (see 331.09 or earlier for token/value record-oriented routines)
 *
 *	It's the responsibility of the caller to make sure the index does not exceed range
 */
/*
uint8_t cmd_read_NVM_value(cmdObj_t *cmd)
{
	int8_t nvm_byte_array[NVM_VALUE_LEN];
	uint16_t nvm_address = cfg.nvm_profile_base + (cmd->index * NVM_VALUE_LEN);
	(void)EEPROM_ReadBytes(nvm_address, nvm_byte_array, NVM_VALUE_LEN);
	memcpy(&cmd->value, &nvm_byte_array, NVM_VALUE_LEN);
	return (SC_OK);
}

uint8_t cmd_write_NVM_value(cmdObj_t *cmd)
{
	double tmp = cmd->value;
	ritorno(cmd_read_NVM_value(cmd));
	if (cmd->value != tmp) {		// catches the isnan() case as well
		cmd->value = tmp;
		int8_t nvm_byte_array[NVM_VALUE_LEN];
		memcpy(&nvm_byte_array, &tmp, NVM_VALUE_LEN);
		uint16_t nvm_address = cfg.nvm_profile_base + (cmd->index * NVM_VALUE_LEN);
		(void)EEPROM_WriteBytes(nvm_address, nvm_byte_array, NVM_VALUE_LEN);
	}
	return (SC_OK);
}
*/
/****************************************************************************
 ***** Config Unit Tests ****************************************************
 ****************************************************************************/

#ifdef __UNIT_TESTS
#ifdef __UNIT_TEST_CONFIG

#define NVMwr(i,v) { cmd.index=i; cmd.value=v; cmd_write_NVM_value(&cmd);}
#define NVMrd(i)   { cmd.index=i; cmd_read_NVM_value(&cmd); printf("%f\n",cmd.value);}

void cfg_unit_tests()
{

// NVM tests
/*	cmdObj_t cmd;
	NVMwr(0, 329.01)
	NVMwr(1, 111.01)
	NVMwr(2, 222.02)
	NVMwr(3, 333.03)
	NVMwr(4, 444.04)
	NVMwr(10, 10.10)
	NVMwr(100, 100.100)
	NVMwr(479, 479.479)

	NVMrd(0)
	NVMrd(1)
	NVMrd(2)
	NVMrd(3)
	NVMrd(4)
	NVMrd(10)
	NVMrd(100)
	NVMrd(479)
*/

// config table tests

	index_t i;
//	double val;

//	_print_configs("$", NUL);					// no filter (show all)
//	_print_configs("$", 'g');					// filter for general parameters
//	_print_configs("$", '1');					// filter for motor 1
//	_print_configs("$", 'x');					// filter for x axis

	i = cmd_get_index("fb");
	i = cmd_get_index("xfr");
	i = cmd_get_index("g54");

//	i = _get_pos_axis(55);
//	i = _get_pos_axis(73);
//	i = _get_pos_axis(93);
//	i = _get_pos_axis(113);

/*
	for (i=0; i<CMD_MAX_INDEX; i++) {

		cmd_get(&cmd);

		cmd.value = 42;
		cmd_set(&cmd);

		val = _get_dbl_value(i);
		cmd_get_token(i, cmd.token);

//		_get_friendly(i, string);
		_get_format(i, cmd.vstring);
		_get_axis(i);							// uncomment main function to test
		_get_motor(i);
		cmd_set(i, &cmd);
		cmd_print(i);
	}

	_parse_config_string("$1po 1", &c);			// returns a number
	_parse_config_string("XFR=1200", &c);		// returns a number
	_parse_config_string("YFR 1300", &c);		// returns a number
	_parse_config_string("zfr	1400", &c);		// returns a number
	_parse_config_string("afr", &c);			// returns a null
	_parse_config_string("Bfr   ", &c);			// returns a null
	_parse_config_string("cfr=wordy", &c);		// returns a null

//	i = cfg_get_config_index("gc");
//	i = cfg_get_config_index("gcode");
//	i = cfg_get_config_index("c_axis_mode");
//	i = cfg_get_config_index("AINT_NOBODY_HOME");
	i = cfg_get_config_index("firmware_version");
*/
}

#endif
#endif

