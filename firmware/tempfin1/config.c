/*
 * config.c - configuration handling and persistence; master function table
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
/*	See config_data.h for an overview of the config system and it's use.
 */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <avr/pgmspace.h>

#include "kinen.h"			// config reaches into almost everything
#include "tempfin1.h"
#include "config.h"
#include "json_parser.h"
#include "report.h"
#include "util.h"
#include "system.h"
#include "xio/xio.h"

/***********************************************************************************
 **** GENERIC STATICS **************************************************************
 ***********************************************************************************
 * Contains function prototypes and other declarations that are not application
 * specific and are needed to support the config_data definitions that follow
 */
typedef char PROGMEM *prog_char_ptr;		// access to PROGMEM arrays of PROGMEM strings

// generic internal functions
static uint8_t _get_nul(cmdObj_t *cmd);		// get null value type
static uint8_t _set_nul(cmdObj_t *cmd);		// set nothing (no operation)
static void _print_nul(cmdObj_t *cmd);		// print nothing (no operation)

static uint8_t _get_ui8(cmdObj_t *cmd);		// get uint8_t value
static uint8_t _set_ui8(cmdObj_t *cmd);		// set uint8_t value
static void _print_ui8(cmdObj_t *cmd);		// print unit8_t value

static uint8_t _get_int(cmdObj_t *cmd);		// get uint32_t integer value
static uint8_t _set_int(cmdObj_t *cmd);		// set uint32_t integer value
static void _print_int(cmdObj_t *cmd);		// print uint32_t integer value

static uint8_t _get_dbl(cmdObj_t *cmd);		// get double value
static uint8_t _set_dbl(cmdObj_t *cmd);		// set double value
static void _print_dbl(cmdObj_t *cmd);		// print double value

static void _print_str(cmdObj_t *cmd);		// print a string value

// generic helper functions
static uint8_t _set_defa(cmdObj_t *cmd);	// reset config to default values
static char *_get_format(const index_t i, char *format);

//static uint8_t _text_parser(char *str, cmdObj_t *c);
//static void _print_text_inline_pairs();
//static void _print_text_inline_values();
//static void _print_text_multiline_formatted();

static uint8_t _set_grp(cmdObj_t *cmd);	// set data for a group
static uint8_t _get_grp(cmdObj_t *cmd);	// get data for a group
//static void _do_group_list(cmdObj_t *cmd, char list[][CMD_TOKEN_LEN+1]); // helper to print multiple groups in a list

/***********************************************************************************
 **** APPLICATION-SPECIFIC DATA ****************************************************
 ***********************************************************************************
 * This section includes the config_data.h file that contains:
 *	- application-specific message and print format strings
 *	- application-specific config array
 *	-,application-specific accessor functions and function prototypes 
 *	- any other application-specific data
 */
#include "config_data.h"

// some evaluators that flow from the data file:
#define _index_is_single(i) ((i <= CMD_INDEX_END_SINGLES) ? true : false)
#define _index_lt_groups(i) ((i <= CMD_INDEX_START_GROUPS) ? true : false)
#define _index_is_group(i) (((i >= CMD_INDEX_START_GROUPS) && (i < CMD_INDEX_START_UBER_GROUPS)) ? true : false)
#define _index_is_uber(i)   ((i >= CMD_INDEX_START_UBER_GROUPS) ? true : false)
#define _index_is_group_or_uber(i) ((i >= CMD_INDEX_START_GROUPS) ? true : false)
uint8_t cmd_index_is_group(index_t index) { return _index_is_group(index);}
//index_t cmd_get_max_index() { return (CMD_INDEX_MAX);}

/***********************************************************************************
 **** CMD FUNCTION ENTRY POINTS ****************************************************
 ***********************************************************************************
 * Primary access points to cmd functions
 * These gatekeeper functions check index ranges so others don't have to
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
	cfg.comm_mode = JSON_MODE;				// initial value until EEPROM is read
//	cfg.nvm_base_addr = NVM_BASE_ADDR;
//	cfg.nvm_profile_base = cfg.nvm_base_addr;
	cmd->value = true;
	_set_defa(cmd);		// this subroutine called from here and from the $defa=1 command
}

static uint8_t _set_defa(cmdObj_t *cmd) 
{
	if (cmd->value != true) { return (SC_OK);}	// failsafe. Must set true or no action occurs
//	rpt_print_initializing_message();
	for (cmd->index=0; _index_is_single(cmd->index); cmd->index++) {
		if (pgm_read_byte(&cfgArray[cmd->index].flags) & F_INITIALIZE) {
			cmd->value = (double)pgm_read_float(&cfgArray[cmd->index].def_value);
			strcpy_P(cmd->token, cfgArray[cmd->index].token);
			cmd_set(cmd);
//			cmd_persist(cmd);
		}
	}
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
	return (SC_OK); // There is no text parser in this code - just JSON
}
/*
	cmdObj_t *cmd = cmd_reset_list();		// returns first object in the body
	uint8_t status = SC_OK;

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

	return (SC_OK);
}

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
 * Generic gets()
 * _get_nul() - get nothing (returns SC_NOOP)
 * _get_ui8() - get value as 8 bit uint8_t w/o unit conversion
 * _get_int() - get value as 32 bit integer w/o unit conversion
 * _get_dbl() - get value as double w/o unit conversion
 *
 * Generic sets()
 * _set_nul() - set nothing (returns SC_NOOP)
 * _set_ui8() - set value as 8 bit uint8_t value w/o unit conversion
 * _set_int() - set value as 32 bit integer w/o unit conversion
 * _set_dbl() - set value as double w/o unit conversion
 *
 * Generic prints()
 * _print_nul() - print nothing
 * _print_ui8() - print uint8_t value w/no units or unit conversion
 * _print_int() - print integer value w/no units or unit conversion
 * _print_dbl() - print double value w/no units or unit conversion
 * _print_str() - print string value
 */

static uint8_t _set_nul(cmdObj_t *cmd) { return (SC_NOOP);}
static void _print_nul(cmdObj_t *cmd) {}
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
static uint8_t _set_ui8(cmdObj_t *cmd)
{
	*((uint8_t *)pgm_read_word(&cfgArray[cmd->index].target)) = cmd->value;
	cmd->type = TYPE_INTEGER;
	return(SC_OK);
}
static void _print_ui8(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), (uint8_t)cmd->value);
}

static uint8_t _get_int(cmdObj_t *cmd)
{
	cmd->value = (double)*((uint32_t *)pgm_read_word(&cfgArray[cmd->index].target));
	cmd->type = TYPE_INTEGER;
	return (SC_OK);
}
static uint8_t _set_int(cmdObj_t *cmd)
{
	*((uint32_t *)pgm_read_word(&cfgArray[cmd->index].target)) = cmd->value;
	cmd->type = TYPE_INTEGER;
	return(SC_OK);
}
static void _print_int(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), (uint32_t)cmd->value);
}

static uint8_t _get_dbl(cmdObj_t *cmd)
{
	cmd->value = *((double *)pgm_read_word(&cfgArray[cmd->index].target));
	cmd->type = TYPE_FLOAT;
	return (SC_OK);
}
static uint8_t _set_dbl(cmdObj_t *cmd)
{
	*((double *)pgm_read_word(&cfgArray[cmd->index].target)) = cmd->value;
	cmd->type = TYPE_FLOAT;
	return(SC_OK);
}
static void _print_dbl(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), cmd->value);
}

static void _print_str(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), *cmd->stringp);
}

/***************************************************************************** 
 * Accessors - get various data from an object given the index
 * _get_format() 	- return format string for an index
 */
static char *_get_format(const index_t i, char *format)
{
	strncpy_P(format, (PGM_P)pgm_read_word(&cfgArray[i].format), CMD_FORMAT_LEN);
	return (format);
}

/********************************************************************************
 * Group operations
 *
 * _get_grp() - read data from a group
 * _set_grp() - get or set one or more values in a group
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

/***********************************************************************************
 ***** cmdObj functions ************************************************************
 ***********************************************************************************/

/*****************************************************************************
 * cmdObj helper functions and other low-level cmd helpers
 * cmd_get_index() 		 - get index from mnenonic token + group
 * cmd_get_type()		 - returns command type as a CMD_TYPE enum
 * cmd_persist_offsets() - write any changed G54 (et al) offsets back to NVM
 * 
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

