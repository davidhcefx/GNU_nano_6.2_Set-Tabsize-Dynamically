/**************************************************************************
 *   history.c  --  This file is part of GNU nano.                        *
 *                                                                        *
 *   Copyright (C) 2003-2011, 2013-2021 Free Software Foundation, Inc.    *
 *   Copyright (C) 2016, 2017, 2019 Benno Schulenberg                     *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "prototypes.h"

#ifdef ENABLE_HISTORIES

#include <errno.h>
#include <string.h>

#ifndef SEARCH_HISTORY
#define SEARCH_HISTORY  "search_history"
#endif

#ifndef POSITION_HISTORY
#define POSITION_HISTORY  "filepos_history"
#endif

static bool history_changed = FALSE;
		/* Whether any of the history lists has changed. */
static char *poshistname = NULL;
		/* The name of the positions-history file. */
static time_t latest_timestamp = 942927132;
		/* The last time the positions-history file was written. */
static poshiststruct *position_history = NULL;
		/* The list of filenames with their last cursor positions. */

/* Initialize the lists of historical search and replace strings
 * and the list of historical executed commands. */
void history_init(void)
{
	search_history = make_new_node(NULL);
	search_history->data = copy_of("");
	searchtop = search_history;
	searchbot = search_history;

	replace_history = make_new_node(NULL);
	replace_history->data = copy_of("");
	replacetop = replace_history;
	replacebot = replace_history;

	execute_history = make_new_node(NULL);
	execute_history->data = copy_of("");
	executetop = execute_history;
	executebot = execute_history;
}

/* Set the current position in the given history list to the bottom. */
void history_reset(const linestruct *list)
{
	if (list == search_history)
		search_history = searchbot;
	else if (list == replace_history)
		replace_history = replacebot;
	else if (list == execute_history)
		execute_history = executebot;
}

/* Return from the history list that starts at start and ends at end
 * the first node that contains the first len characters of the given
 * text, or NULL if there is no such node. */
linestruct *find_history(const linestruct *start, const linestruct *end,
		const char *text, size_t len)
{
	const linestruct *item;

	for (item = start; item != end->prev && item != NULL; item = item->prev) {
		if (strncmp(item->data, text, len) == 0)
			return (linestruct *)item;
	}

	return NULL;
}

/* Update a history list (the one in which item is the current position)
 * with a fresh string text.  That is: add text, or move it to the end. */
void update_history(linestruct **item, const char *text)
{
	linestruct **htop = NULL, **hbot = NULL, *thesame;

	if (*item == search_history) {
		htop = &searchtop;
		hbot = &searchbot;
	} else if (*item == replace_history) {
		htop = &replacetop;
		hbot = &replacebot;
	} else if (*item == execute_history) {
		htop = &executetop;
		hbot = &executebot;
	}

	/* See if the string is already in the history. */
	thesame = find_history(*hbot, *htop, text, HIGHEST_POSITIVE);

	/* If an identical string was found, delete that item. */
	if (thesame != NULL) {
		linestruct *after = thesame->next;

		/* If the string is at the head of the list, move the head. */
		if (thesame == *htop)
			*htop = after;

		unlink_node(thesame);
		renumber_from(after);
	}

	/* If the history is full, delete the oldest item (the one at the
	 * head of the list), to make room for a new item at the end. */
	if ((*hbot)->lineno == MAX_SEARCH_HISTORY + 1) {
		linestruct *oldest = *htop;

		*htop = (*htop)->next;
		unlink_node(oldest);
		renumber_from(*htop);
	}

	/* Store the fresh string in the last item, then create a new item. */
	(*hbot)->data = mallocstrcpy((*hbot)->data, text);
	splice_node(*hbot, make_new_node(*hbot));
	*hbot = (*hbot)->next;
	(*hbot)->data = copy_of("");

	/* Indicate that the history needs to be saved on exit. */
	history_changed = TRUE;

	/* Set the current position in the list to the bottom. */
	*item = *hbot;
}

/* Move h to the string in the history list just before it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_older(linestruct **h)
{
	if ((*h)->prev == NULL)
		return NULL;

	*h = (*h)->prev;

	return (*h)->data;
}

/* Move h to the string in the history list just after it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_newer(linestruct **h)
{
	if ((*h)->next == NULL)
		return NULL;

	*h = (*h)->next;

	return (*h)->data;
}

/* Two empty placeholder functions. */
void get_history_older_void(void)
{
}
void get_history_newer_void(void)
{
}

#ifdef ENABLE_TABCOMP
/* Go backward through one of three history lists, starting at its item h,
 * searching for a string that is a tab completion of the given string s,
 * looking at only the first len characters of s.  When found, make h point
 * at it and return that string; otherwise, don't move h and return s. */
char *get_history_completion(linestruct **h, char *s, size_t len)
{
	linestruct *htop = NULL, *hbot = NULL, *p;

	if (*h == search_history) {
		htop = searchtop;
		hbot = searchbot;
	} else if (*h == replace_history) {
		htop = replacetop;
		hbot = replacebot;
	} else if (*h == execute_history) {
		htop = executetop;
		hbot = executebot;
	}

	/* First search from the current position to the top of the list
	 * for a match of len characters.  Skip over an exact match. */
	p = find_history((*h)->prev, htop, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
		p = find_history(p->prev, htop, s, len);

	if (p != NULL) {
		*h = p;
		return mallocstrcpy(s, (*h)->data);
	}

	/* Now search from the bottom of the list to the original position. */
	p = find_history(hbot, *h, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
		p = find_history(p->prev, *h, s, len);

	if (p != NULL) {
		*h = p;
		return mallocstrcpy(s, (*h)->data);
	}

	/* When no useful match was found, simply return the given string. */
	return (char *)s;
}
#endif /* ENABLE_TABCOMP */

/* Check whether we have or could make a directory for history files. */
bool have_statedir(void)
{
	struct stat dirstat;
	const char *xdgdatadir;

	get_homedir();

	if (homedir != NULL) {
		statedir = concatenate(homedir, "/.nano/");

		if (stat(statedir, &dirstat) == 0 && S_ISDIR(dirstat.st_mode)) {
			poshistname = concatenate(statedir, POSITION_HISTORY);
			return TRUE;
		}
	}

	free(statedir);
	xdgdatadir = getenv("XDG_DATA_HOME");

	if (homedir == NULL && xdgdatadir == NULL)
		return FALSE;

	if (xdgdatadir != NULL)
		statedir = concatenate(xdgdatadir, "/nano/");
	else
		statedir = concatenate(homedir, "/.local/share/nano/");

	if (stat(statedir, &dirstat) == -1) {
		if (xdgdatadir == NULL) {
			char *statepath = concatenate(homedir, "/.local");
			mkdir(statepath, S_IRWXU | S_IRWXG | S_IRWXO);
			free(statepath);
			statepath = concatenate(homedir, "/.local/share");
			mkdir(statepath, S_IRWXU);
			free(statepath);
		}
		if (mkdir(statedir, S_IRWXU) == -1) {
			jot_error(N_("Unable to create directory %s: %s\n"
								"It is required for saving/loading "
								"search history or cursor positions.\n"),
								statedir, strerror(errno));
			return FALSE;
		}
	} else if (!S_ISDIR(dirstat.st_mode)) {
		jot_error(N_("Path %s is not a directory and needs to be.\n"
								"Nano will be unable to load or save "
								"search history or cursor positions.\n"),
								statedir);
		return FALSE;
	}

	poshistname = concatenate(statedir, POSITION_HISTORY);
	return TRUE;
}

/* Load the histories for Search, Replace With, and Execute Command. */
void load_history(void)
{
	char *histname = concatenate(statedir, SEARCH_HISTORY);
	FILE *histfile = fopen(histname, "rb");

	/* If reading an existing file failed, don't save history when we quit. */
	if (histfile == NULL && errno != ENOENT) {
		jot_error(N_("Error reading %s: %s"), histname, strerror(errno));
		UNSET(HISTORYLOG);
	}

	if (histfile == NULL) {
		free(histname);
		return;
	}

	linestruct **history = &search_history;
	char *stanza = NULL;
	size_t dummy = 0;
	ssize_t read;

	/* Load the three history lists (first search, then replace, then execute)
	 * from oldest entry to newest.  Between two lists there is an empty line. */
	while ((read = getline(&stanza, &dummy, histfile)) > 0) {
		stanza[--read] = '\0';
		if (read > 0) {
			recode_NUL_to_LF(stanza, read);
			update_history(history, stanza);
		} else if (history == &search_history)
			history = &replace_history;
		else
			history = &execute_history;
	}

	if (fclose(histfile) == EOF)
		jot_error(N_("Error reading %s: %s"), histname, strerror(errno));

	free(histname);
	free(stanza);

	/* Reading in the lists has marked them as changed; undo this side effect. */
	history_changed = FALSE;
}

/* Write the lines of a history list, starting at head, from oldest to newest,
 * to the given file.  Return TRUE if writing succeeded, and FALSE otherwise. */
bool write_list(const linestruct *head, FILE *histfile)
{
	const linestruct *item;

	for (item = head; item != NULL; item = item->next) {
		size_t length = strlen(item->data);

		/* Decode 0x0A bytes as embedded NULs. */
		recode_LF_to_NUL(item->data);

		if (fwrite(item->data, sizeof(char), length, histfile) < length)
			return FALSE;
		if (putc('\n', histfile) == EOF)
			return FALSE;
	}

	return TRUE;
}

/* Save the histories for Search, Replace With, and Execute Command. */
void save_history(void)
{
	char *histname;
	FILE *histfile;

	/* If the histories are unchanged, don't bother saving them. */
	if (!history_changed)
		return;

	histname = concatenate(statedir, SEARCH_HISTORY);
	histfile = fopen(histname, "wb");

	if (histfile == NULL) {
		jot_error(N_("Error writing %s: %s"), histname, strerror(errno));
		free(histname);
		return;
	}

	/* Don't allow others to read or write the history file. */
	if (chmod(histname, S_IRUSR | S_IWUSR) < 0)
		jot_error(N_("Cannot limit permissions on %s: %s"), histname, strerror(errno));

	if (!write_list(searchtop, histfile) || !write_list(replacetop, histfile) ||
											!write_list(executetop, histfile))
		jot_error(N_("Error writing %s: %s"), histname, strerror(errno));

	if (fclose(histfile) == EOF)
		jot_error(N_("Error writing %s: %s"), histname, strerror(errno));

	free(histname);
}

/* Load the recorded cursor positions for files that were edited. */
void load_poshistory(void)
{
	FILE *histfile = fopen(poshistname, "rb");

	/* If reading an existing file failed, don't save history when we quit. */
	if (histfile == NULL && errno != ENOENT) {
		jot_error(N_("Error reading %s: %s"), poshistname, strerror(errno));
		UNSET(POSITIONLOG);
	}

	if (histfile == NULL)
		return;

	ssize_t read, count = 0;
	struct stat fileinfo;
	poshiststruct *record_ptr = NULL, *newrecord;
	char *lineptr, *columnptr;
	char *stanza = NULL;
	size_t dummy = 0;

	/* Read and parse each line, and store the extracted data. */
	while ((read = getline(&stanza, &dummy, histfile)) > 5) {
		/* Decode NULs as embedded newlines. */
		recode_NUL_to_LF(stanza, read);

		/* Find the spaces before column number and line number. */
		columnptr = revstrstr(stanza, " ", stanza + read - 3);
		if (columnptr == NULL)
			continue;
		lineptr = revstrstr(stanza, " ", columnptr - 2);
		if (lineptr == NULL)
			continue;

		/* Now separate the three elements of the line. */
		*(columnptr++) = '\0';
		*(lineptr++) = '\0';

		/* Create a new position record. */
		newrecord = nmalloc(sizeof(poshiststruct));
		newrecord->filename = copy_of(stanza);
		newrecord->linenumber = atoi(lineptr);
		newrecord->columnnumber = atoi(columnptr);
		newrecord->next = NULL;

		/* Add the record to the list. */
		if (position_history == NULL)
			position_history = newrecord;
		else
			record_ptr->next = newrecord;

		record_ptr = newrecord;

		/* Impose a limit, so the file will not grow indefinitely. */
		if (++count > 200) {
			poshiststruct *drop_record = position_history;

			position_history = position_history->next;

			free(drop_record->filename);
			free(drop_record);
		}
	}

	if (fclose(histfile) == EOF)
		jot_error(N_("Error reading %s: %s"), poshistname, strerror(errno));

	free(stanza);

	if (stat(poshistname, &fileinfo) == 0)
		latest_timestamp = fileinfo.st_mtime;
}

/* Save the recorded cursor positions for files that were edited. */
void save_poshistory(void)
{
	poshiststruct *posptr;
	struct stat fileinfo;
	FILE *histfile = fopen(poshistname, "wb");

	if (histfile == NULL) {
		jot_error(N_("Error writing %s: %s"), poshistname, strerror(errno));
		return;
	}

	/* Don't allow others to read or write the history file. */
	if (chmod(poshistname, S_IRUSR | S_IWUSR) < 0)
		jot_error(N_("Cannot limit permissions on %s: %s"), poshistname, strerror(errno));

	for (posptr = position_history; posptr != NULL; posptr = posptr->next) {
		char *path_and_place;
		size_t length;

		/* Assume 20 decimal positions each for line and column number,
		 * plus two spaces, plus the line feed, plus the null byte. */
		path_and_place = nmalloc(strlen(posptr->filename) + 44);
		sprintf(path_and_place, "%s %zd %zd\n",
								posptr->filename, posptr->linenumber, posptr->columnnumber);
		length = strlen(path_and_place);

		/* Encode newlines in filenames as NULs. */
		recode_LF_to_NUL(path_and_place);
		/* Restore the terminating newline. */
		path_and_place[length - 1] = '\n';

		if (fwrite(path_and_place, sizeof(char), length, histfile) < length)
			jot_error(N_("Error writing %s: %s"), poshistname, strerror(errno));

		free(path_and_place);
	}

	if (fclose(histfile) == EOF)
		jot_error(N_("Error writing %s: %s"), poshistname, strerror(errno));

	if (stat(poshistname, &fileinfo) == 0)
		latest_timestamp = fileinfo.st_mtime;
}

/* Reload the position history file if it has been modified since last load. */
void reload_positions_if_needed(void)
{
	struct stat fileinfo;

	if (stat(poshistname, &fileinfo) == 0 && fileinfo.st_mtime != latest_timestamp) {
		poshiststruct *item, *nextone;

		for (item = position_history; item != NULL; item = nextone) {
			nextone = item->next;
			free(item->filename);
			free(item);
		}

		position_history = NULL;

		load_poshistory();
	}
}

/* Update the recorded last file positions with the current position in the
 * current buffer.  If no existing entry is found, add a new one at the end. */
void update_poshistory(void)
{
	poshiststruct *posptr, *theone, *posprev = NULL;
	char *fullpath = get_full_path(openfile->filename);

	if (fullpath == NULL || openfile->filename[0] == '\0') {
		free(fullpath);
		return;
	}

	reload_positions_if_needed();

	/* Look for a matching filename in the list. */
	for (posptr = position_history; posptr != NULL; posptr = posptr->next) {
		if (!strcmp(posptr->filename, fullpath))
			break;
		posprev = posptr;
	}

	/* Don't record files that have the default cursor position. */
	if (openfile->current->lineno == 1 && openfile->current_x == 0) {
		if (posptr != NULL) {
			if (posprev == NULL)
				position_history = posptr->next;
			else
				posprev->next = posptr->next;
			free(posptr->filename);
			free(posptr);
			save_poshistory();
		}
		free(fullpath);
		return;
	}

	theone = posptr;

	/* If we didn't find it, make a new node; otherwise, if we're
	 * not at the end, move the matching one to the end. */
	if (theone == NULL) {
		theone = nmalloc(sizeof(poshiststruct));
		theone->filename = copy_of(fullpath);
		if (position_history == NULL)
			position_history = theone;
		else
			posprev->next = theone;
	} else if (posptr->next != NULL) {
		if (posprev == NULL)
			position_history = posptr->next;
		else
			posprev->next = posptr->next;
		while (posptr->next != NULL)
			posptr = posptr->next;
		posptr->next = theone;
	}

	/* Store the last cursor position. */
	theone->linenumber = openfile->current->lineno;
	theone->columnnumber = xplustabs() + 1;
	theone->next = NULL;

	free(fullpath);

	save_poshistory();
}

/* Check whether the given file matches an existing entry in the recorded
 * last file positions.  If not, return FALSE.  If yes, return TRUE and
 * set line and column to the retrieved values. */
bool has_old_position(const char *file, ssize_t *line, ssize_t *column)
{
	poshiststruct *posptr;
	char *fullpath = get_full_path(file);

	if (fullpath == NULL)
		return FALSE;

	reload_positions_if_needed();

	posptr = position_history;
	while (posptr != NULL && strcmp(posptr->filename, fullpath) != 0)
		posptr = posptr->next;

	free(fullpath);

	if (posptr == NULL)
		return FALSE;

	*line = posptr->linenumber;
	*column = posptr->columnnumber;
	return TRUE;
}
#endif /* ENABLE_HISTORIES */
