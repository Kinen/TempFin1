/*
 * config_textmode.c - text mode support for config sub-system
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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <avr/pgmspace.h>

#include "kinen.h"			// config reaches into almost everything
#include "tempfin.h"
#include "config.h"
#include "config_app.h"
#include "json_parser.h"
#include "report.h"
#include "util.h"
#include "system.h"
#include "xio/xio.h"

extern const cfgItem_t cfgArray[];
static uint8_t _text_parser(char *str, cmdObj_t *c);

/***********************************************************************************
 **** CMD FUNCTION ENTRY POINTS ****************************************************
 ***********************************************************************************
 * Primary access points to cmd functions
 * These gatekeeper functions check index ranges so others don't have to
 *
 * cmd_print()	- Output a formatted string for the value.
 */
void cmd_print(cmdObj_t *cmd)
{
//	if (cmd->index >= CMD_INDEX_MAX) return;
	((fptrPrint)(pgm_read_word(&cfgArray[cmd->index].print)))(cmd);
}

/****************************************************************************
 * cmd_text_parser() - update a config setting from a text block (text mode)
 * _text_parser() 	 - helper for above
 * 
 * Use cases handled:
 *	- $xfr=1200	set a parameter
 *	- $xfr		display a parameter
 *	- $x		display a group
 *	- ?			generate a status report (multiline format)
 */
uint8_t cmd_text_parser(char *str)
{
//	return (SC_OK); // There is no text parser in this code - just JSON
//}

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


/***************************************************************************** 
 * Accessors - get various data from an object given the index
 * _get_format() 	- return format string for an index
 */
char *_get_format(const index_t i, char *format)
{
	strncpy_P(format, (PGM_P)pgm_read_word(&cfgArray[i].format), CMD_FORMAT_LEN);
	return (format);
}

/***** Generic Internal Functions *******************************************
 * Generic prints()
 * _print_nul() - print nothing
 * _print_ui8() - print uint8_t value w/no units or unit conversion
 * _print_int() - print integer value w/no units or unit conversion
 * _print_dbl() - print double value w/no units or unit conversion
 * _print_str() - print string value
 */
void _print_nul(cmdObj_t *cmd) {}

void _print_ui8(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), (uint8_t)cmd->value);
}
void _print_int(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), (uint32_t)cmd->value);
}
void _print_dbl(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), cmd->value);
}
void _print_str(cmdObj_t *cmd)
{
	cmd_get(cmd);
	char format[CMD_FORMAT_LEN+1];
	fprintf(stderr, _get_format(cmd->index, format), *cmd->stringp);
}

/****************************************************************************
 * void cmd_print_text_inline_pairs()
 * void cmd_print_text_inline_values()
 * void cmd_print_text_multiline_formatted()
 */
void cmd_print_text_inline_pairs()
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

void cmd_print_text_inline_values()
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

void cmd_print_text_multiline_formatted()
{
	cmdObj_t *cmd = cmd_body;

	for (uint8_t i=0; i<CMD_BODY_LEN-1; i++) {
		if (cmd->type != TYPE_PARENT) { cmd_print(cmd);}
		cmd = cmd->nx;
		if (cmd->type == TYPE_EMPTY) { break;}
	}
}


