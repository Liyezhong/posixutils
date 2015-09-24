/*
 * Copyright 2004-2006 Jeff Garzik <jgarzik@pobox.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#ifndef HAVE_CONFIG_H
#error missing autoconf-generated config.h.
#endif
#include "posixutils-config.h"

#include <string.h>
#include <vector>
#include <string>
#include <algorithm>
#include <argp.h>
#include <libpu.h>

using namespace std;

static const char doc[] =
N_("sort - sort, merge, or sequence check text files");

#define args_doc file_args_doc

static struct argp_option options[] = {
	{ NULL, 'c', NULL, 0,
	  N_("Check that the single input file is ordered as specified") },
	{ NULL, 'm', NULL, 0,
	  N_("Merge only; the input file shall be assumed to be already sorted.") },
	{ NULL, 'o', "FILE", 0,
	  N_("Specify the name of an output file to be used instead of the standard output") },
	{ NULL, 'u', NULL, 0,
	  N_("Unique: suppress all but one in each set of lines having equal keys.") },
	{ NULL, 'd', NULL, 0,
	  N_("Specify that only <blank>s and alphanumeric characters shall be significant in comparisons") },
	{ NULL, 'f', NULL, 0,
	  N_("Force lowercase to uppercase, for comparison") },
	{ NULL, 'i', NULL, 0,
	  N_("Ignore all characters that are non-printable") },
	{ NULL, 'n', NULL, 0,
	  N_("Restrict the sort key to an initial numeric string") },
	{ NULL, 'r', NULL, 0,
	  N_("Reverse the sense of comparisons") },
	{ NULL, 'b', NULL, 0,
	  N_("Ignore leading <blank>s when determining the starting and ending positions of a restricted sort key") },
	{ NULL, 't', "CHAR", 0,
	  N_("Use char as the field separator character") },
	{ NULL, 'k', "KEYDEF", 0,
	  N_("The keydef argument is a restricted sort key field definition") },
	{ }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static const struct argp argp = { options, parse_opt, args_doc, doc };

static struct walker walker;

class SortLine {
public:
	string line;

	SortLine() {}
	SortLine(const char *s) : line(s) {
	}
};

static enum opt_mode_type {
	MODE_SORT,
	MODE_MERGE,
	MODE_CHECK,
} opt_mode = MODE_SORT;
static const char *opt_output = "-";
static bool opt_unique = false;
static bool opt_alphanum = false;
static bool opt_force_upper = false;
static bool opt_ignore_nonprint = false;
static bool opt_numeric = false;
static bool opt_reverse = false;
static bool opt_ignore_lblank = false;
static int opt_separator = -1;
static vector<string> opt_keydef;
static vector<SortLine> lines;

static bool line_compare(const SortLine& a, const SortLine& b)
{
	int rc = strcoll(a.line.c_str(), b.line.c_str());

	if (opt_reverse)
		rc = -rc;

	return (rc <= 0);
}

static bool check_line(const SortLine& line)
{
	static bool have_prev = false;
	static SortLine prev_line;

	if (!have_prev) {
		prev_line = line;
		have_prev = true;
		return true;
	}

	bool cmp_rc = line_compare(prev_line, line);

	prev_line = line;

	return cmp_rc;
}

static int sort_actor(struct walker *w, const char *fn, FILE *f)
{
	char *c_line, linebuf[4096];

	while ((c_line = fgets_unlocked(linebuf, sizeof(linebuf), f)) != NULL){
		SortLine line(c_line);

		if (opt_mode == MODE_CHECK) {
			if (!check_line(line)) {
				w->exit_status = 1;
				return RC_STOP_WALK;
			}
		} else
			lines.push_back(line);
	}

	if (ferror(f)) {
		perror(fn);
		w->exit_status = 1;
	}
	
	return RC_OK;
}

static int sort_post_walk(struct walker *w)
{
	if (opt_mode != MODE_SORT)
		return w->exit_status;

	std::sort(lines.begin(), lines.end(), line_compare);

	for (vector<SortLine>::iterator it = lines.begin();
	     it != lines.end(); ++it)
		printf("%s", (*it).line.c_str());

	return w->exit_status;
}

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	PU_OPT_BEGIN

	case 'c':
		opt_mode = MODE_CHECK;
		break;
	case 'm':
		opt_mode = MODE_MERGE;
		break;
	case 'o':
		opt_output = arg;
		break;
	case 't':
		if (strlen(arg) > 1)
			argp_usage (state);
		opt_separator = arg[0];
		break;
	case 'k':
		opt_keydef.push_back(arg);
		break;

	PU_OPT_SET('u', unique)
	PU_OPT_SET('d', alphanum)
	PU_OPT_SET('f', force_upper)
	PU_OPT_SET('i', ignore_nonprint)
	PU_OPT_SET('n', numeric)
	PU_OPT_SET('r', reverse)
	PU_OPT_SET('b', ignore_lblank)
	PU_OPT_ARG
	PU_OPT_DEFAULT
	PU_OPT_END
}

int main (int argc, char *argv[])
{
	walker.argp			= &argp;
	walker.flags			= WF_NO_FILES_STDIN | WF_STDIN_DASH;
	walker.post_walk		= sort_post_walk;
	walker.cmdline_file		= sort_actor;
	return walk(&walker, argc, argv);
}
