/*
 * config_data.h - application-specific configuration data
 * Part of Kinen project
 *
 * Copyright (c) 2010 - 2013 Alden S. Hart Jr.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* This file contains application speoific data for the config system:
 *	-,application-specific functions and function prototypes 
 *	- application-specific message and print format strings
 *	- application-specific config array
 *	- any other application-specific data
 */
/*
 * --- Config objects and the config list ---
 *
 *	The config system provides a structured way to access and set configuration variables
 *	and to invoke commands and functions from the command line and from JSON input.
 *	It also provides a way to get to an arbitrary variable for reading or display.
 *
 *	Config operates as a collection of "objects" (OK, so they are not really objects) 
 *	that encapsulate each variable. The objects are collected into a list (a body) 
 *	which may also have header and footer objects. 
 *
 *	This way the internals don't care about how the variable is represented or 
 *	communicated externally as all operations occur on the cmdObj list. The list is 
 *	populated by the text_parser or the JSON_parser depending on the mode. Lists 
 *	are also used for responses and are read out (printed) by a text-mode or JSON 
 *	print functions.
 */
/* --- Config variables, tables and strings ---
 *
 *	Each configuration value is identified by a short mnemonic string (token). The token 
 *	is resolved to an index into the cfgArray which is an program memory (PROGMEM) array 
 *	of structures with the static assignments for each variable. The array is organized as:
 * 
 *	  - group string identifying what group the variable is part of (if any)
 *	  - token string - the token for that variable - pre-pended with the group (if any)
 *	  - operations flags - flag if the value should be initialized, persisted, etc.
 *	  - pointer to a formatted print string also in program memory (Used only for text mode)
 *	  - function pointer for formatted print() method or text-mode readouts
 *	  - function pointer for get() method - gets values from memory
 *	  - function pointer for set() method - sets values and runs functions
 *	  - target - memory location that the value is written to / read from
 *	  - default value - for cold initialization
 *
 *	Persistence is provided by an NVM array containing values in EEPROM as doubles; 
 *	indexed by cfgArray index
 *
 *	The following rules apply to mnemonic tokens
 *	 - are up to 5 alphnuneric characters and cannot contain whitespace or separators
 *	 - must be unique (non colliding).
 *
 *  "Groups" are collections of values that mimic REST composite resources. Groups include:
 *	 - a system group is identified by "sys" and contains a collection of otherwise unrelated values
 *
 *	"Uber-groups" are groups of groups that are only used for text-mode printing - e.g.
 *	 - group of all groups
 */
/* --- Making changes and adding new values
 *
 *	Adding a new value to config (or changing an existing one) involves touching the following places:
 *
 *	 - Add a formatting string to fmt_XXX strings. Not needed if there is no text-mode print function
 *	   of you are using one of the generic print format strings.
 * 
 *	 - Create a new record in cfgArray[]. Use existing ones for examples. You can usually use 
 *	   generic functions for get and set; or create a new one if you need a specialized function.
 *
 *	   The ordering of group displays is set by the order of items in cfgArray. None of the other 
 *	   orders matter but are generally kept sequenced for easier reading and code maintenance. Also,
 *	   Items earlier in the array will resolve token searches faster than ones later in the array.
 *
 *	   Note that matching will occur from the most specific to the least specific, meaning that
 *	   if tokens overlap the longer one should be earlier in the array: "gco" should precede "gc".
 */
/* --- Rules, guidelines and random stuff
 *
 *	It's the responsibility of the object creator to set the index in the cmdObj when a variable
 *	is "hydrated". Many downstream function expect a valid index int he cmdObj struct. Set the 
 *	index by calling cmd_get_index(). This also validates the token and group if no lookup exists.
 */

/***********************************************************************************
 **** MORE INCLUDE FILES ***********************************************************
 ***********************************************************************************
 * Depending on what's in your functions you may require more include files here
 */
//#include <ctype.h>
//#include <stdlib.h>
//#include <math.h>

/***********************************************************************************
 **** APPLICATION_SPECIFIC CONFIG STRUCTURE(S) *************************************
 ***********************************************************************************
 * Define the cfg structures(s) used by the application
 */
typedef struct cfgParameters {
	double fw_build;				// tinyg firmware build number
	double fw_version;				// tinyg firmware version number
	double hw_version;				// tinyg hardware compatibility

	uint16_t nvm_base_addr;			// NVM base address
	uint16_t nvm_profile_base;		// NVM base address of current profile

	// communications settings		// these first 4 are shadow settigns for XIO cntrl bits
//	uint8_t ignore_crlf;			// ignore CR or LF on RX
//	uint8_t enable_cr;				// enable CR in CRFL expansion on TX
//	uint8_t enable_echo;			// enable text-mode echo
//	uint8_t enable_xon;				// enable XON/XOFF mode
	uint8_t comm_mode;				// TG_TEXT_MODE or TG_JSON_MODE

//	uint8_t json_verbosity;			// see enum in this file for settings
//	uint8_t text_verbosity;			// see enum in this file for settings
//	uint8_t usb_baud_rate;			// see xio_usart.h for XIO_BAUD values
//	uint8_t usb_baud_flag;			// technically this belongs in the controller singleton

//	uint8_t echo_json_footer;		// flags for JSON responses serialization
//	uint8_t echo_json_configs;
//	uint8_t echo_json_messages;
//	uint8_t echo_json_linenum;
//	uint8_t echo_json_gcode_block;

//	uint8_t echo_text_prompt;		// flags for text mode response construction
//	uint8_t echo_text_messages;
//	uint8_t echo_text_configs;
//	uint8_t echo_text_gcode_block;

	// status report configs
//	uint8_t status_report_verbosity;					// see enum in this file for settings
//	uint32_t status_report_interval;					// in MS. set non-zero to enable
//	index_t status_report_list[CMD_STATUS_REPORT_LEN];	// status report elements to report
//	double status_report_value[CMD_STATUS_REPORT_LEN];	// previous values for filtered reporting

} cfgParameters_t;
cfgParameters_t cfg; 				// declared in the header to make it global

/***********************************************************************************
 **** PROGRAM MEMORY STRINGS AND STRING ARRAYS *************************************
 ***********************************************************************************/

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
//static const char fmt_id[] PROGMEM = "[id]  TinyG ID%30s\n";

/***********************************************************************************
 **** FUNCTIONS AND PROTOTYPES *****************************************************
 ***********************************************************************************/

/*
static uint8_t _set_hv(cmdObj_t *cmd);		// set hardware version
static uint8_t _get_sr(cmdObj_t *cmd);		// run status report (as data)
static void _print_sr(cmdObj_t *cmd);		// run status report (as printout)
static uint8_t _set_sr(cmdObj_t *cmd);		// set status report specification
static uint8_t _set_si(cmdObj_t *cmd);		// set status report interval
static uint8_t _get_id(cmdObj_t *cmd);		// get device ID
static uint8_t _get_qr(cmdObj_t *cmd);		// run queue report (as data)
static uint8_t _get_rx(cmdObj_t *cmd);		// get bytes in RX buffer

static uint8_t _set_ic(cmdObj_t *cmd);		// ignore CR or LF on RX input
static uint8_t _set_ec(cmdObj_t *cmd);		// expand CRLF on TX outout
static uint8_t _set_ee(cmdObj_t *cmd);		// enable character echo
static uint8_t _set_ex(cmdObj_t *cmd);		// enable XON/XOFF
static uint8_t _set_baud(cmdObj_t *cmd);	// set USB baud rate
*/

/***********************************************************************************
 **** CONFIG ARRAY *****************************************************************
 ***********************************************************************************
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
 *
 *	NOTE: If the count of lines in cfgArray exceeds 255 you need to change index_t 
 *	uint16_t in the config.h file.
 */

const cfgItem_t cfgArray[] PROGMEM = {
	// grp  token flags format*, print_func, get_func, set_func  target for get/set,   default value
	{ "sys","fb", _f07, fmt_fb, _print_dbl, _get_dbl, _set_dbl, (double *)&cfg.fw_build,   BUILD_NUMBER }, // MUST BE FIRST!
	{ "sys","fv", _f07, fmt_fv, _print_dbl, _get_dbl, _set_dbl, (double *)&cfg.fw_version, VERSION_NUMBER },
	{ "sys","hv", _f07, fmt_hv, _print_dbl, _get_dbl, _set_dbl, (double *)&cfg.hw_version, HARDWARE_VERSION },
//	{ "sys","id", _fns, fmt_id, _print_str, _get_id,  _set_nul, (double *)&kc.null, 0 },		// device ID (ASCII signature)

	// Reports, tests, help, and messages
//	{ "", "sr",  _f00, fmt_nul, _print_sr,  _get_sr,  _set_sr,  (double *)&kc.null, 0 },		// status report object
//	{ "", "qr",  _f00, fmt_qr,  _print_int, _get_qr,  _set_nul, (double *)&kc.null, 0 },		// queue report setting
//	{ "", "rx",  _f00, fmt_rx,  _print_int, _get_rx,  _set_nul, (double *)&kc.null, 0 },		// space in RX buffer
//	{ "", "msg", _f00, fmt_str, _print_str, _get_nul, _set_nul, (double *)&kc.null, 0 },		// string for generic messages
//	{ "", "test",_f00, fmt_nul, _print_nul, print_test_help, tg_test, (double *)&kc.test,0 },	// prints test help screen
//	{ "", "defa",_f00, fmt_nul, _print_nul, print_defaults_help,_set_defa,(double *)&kc.null,0},// prints defaults help screen
//	{ "", "boot",_f00, fmt_nul, _print_nul, print_boot_loader_help,_set_nul, (double *)&kc.null,0 },
//	{ "", "help",_f00, fmt_nul, _print_nul, print_config_help,_set_nul, (double *)&kc.null,0 },	// prints config help screen
//	{ "", "h",   _f00, fmt_nul, _print_nul, print_config_help,_set_nul, (double *)&kc.null,0 },	// alias for "help"

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

	// Persistence for status report - must be in sequence
	// *** Count must agree with CMD_STATUS_REPORT_LEN in config.h ***
//	{ "","se00",_fpe, fmt_nul, _print_nul, _get_int, _set_int,(double *)&cfg.status_report_list[0],0 },
//	{ "","se01",_fpe, fmt_nul, _print_nul, _get_int, _set_int,(double *)&cfg.status_report_list[1],0 },
//	{ "","se02",_fpe, fmt_nul, _print_nul, _get_int, _set_int,(double *)&cfg.status_report_list[2],0 },
//	{ "","se03",_fpe, fmt_nul, _print_nul, _get_int, _set_int,(double *)&cfg.status_report_list[3],0 },

	// Group lookups - must follow the single-valued entries for proper sub-string matching
	// *** Must agree with CMD_COUNT_GROUPS below ****
	{ "","sys",_f00, fmt_nul, _print_nul, _get_grp, _set_grp,(double *)&kc.null,0 },	// system group
	{ "","sys",_f00, fmt_nul, _print_nul, _get_grp, _set_grp,(double *)&kc.null,0 },	// system group

	// Uber-group (groups of groups, for text-mode displays only)
	// *** Must agree with CMD_COUNT_UBER_GROUPS below ****
	{ "", "$", _f00, fmt_nul, _print_nul, _get_nul, _set_nul,(double *)&kc.null,0 }
};

#define CMD_COUNT_GROUPS 		1		// count of simple groups
#define CMD_COUNT_UBER_GROUPS 	0 		// count of uber-groups

#define CMD_INDEX_MAX (sizeof cfgArray / sizeof(cfgItem_t))
//#define CMD_INDEX_END_SINGLES		(CMD_INDEX_MAX - CMD_COUNT_UBER_GROUPS - CMD_COUNT_GROUPS - CMD_STATUS_REPORT_LEN)
#define CMD_INDEX_END_SINGLES		(CMD_INDEX_MAX - CMD_COUNT_UBER_GROUPS - CMD_COUNT_GROUPS)
#define CMD_INDEX_START_GROUPS		(CMD_INDEX_MAX - CMD_COUNT_UBER_GROUPS - CMD_COUNT_GROUPS)
#define CMD_INDEX_START_UBER_GROUPS (CMD_INDEX_MAX - CMD_COUNT_UBER_GROUPS)
