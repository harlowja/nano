/* $Id$ */
/**************************************************************************
 *   text.c                                                              *
 *                                                                        *
 *   Copyright (C) 2005 Chris Allegretta                                  *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 2, or (at your option)  *
 *   any later version.                                                   *
 *                                                                        *
 *   This program is distributed in the hope that it will be useful, but  *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    *
 *   General Public License for more details.                             *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program; if not, write to the Free Software          *
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA            *
 *   02110-1301, USA.                                                     *
 *                                                                        *
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include "proto.h"

#ifndef NANO_SMALL
static pid_t pid;		/* The PID of the newly forked process
				 * in execute_command().  It must be
				 * global because the cancel_command()
				 * signal handler needs it. */
#endif
#ifndef DISABLE_WRAPPING
static bool same_line_wrap = FALSE;
	/* Should we prepend wrapped text to the next line? */
#endif
#ifndef DISABLE_JUSTIFY
static filestruct *jusbottom = NULL;
	/* Pointer to end of justify buffer. */
#endif

#ifndef NANO_SMALL
void do_mark(void)
{
    openfile->mark_set = !openfile->mark_set;
    if (openfile->mark_set) {
	statusbar(_("Mark Set"));
	openfile->mark_begin = openfile->current;
	openfile->mark_begin_x = openfile->current_x;
    } else {
	statusbar(_("Mark UNset"));
	openfile->mark_begin = NULL;
	openfile->mark_begin_x = 0;
	edit_refresh();
    }
}
#endif /* !NANO_SMALL */

void do_delete(void)
{
    bool do_refresh = FALSE;
	/* Do we have to call edit_refresh(), or can we get away with
	 * update_line()? */

    assert(openfile->current != NULL && openfile->current->data != NULL && openfile->current_x <= strlen(openfile->current->data));

    openfile->placewewant = xplustabs();

    if (openfile->current->data[openfile->current_x] != '\0') {
	int char_buf_len = parse_mbchar(openfile->current->data +
		openfile->current_x, NULL, NULL);
	size_t line_len = strlen(openfile->current->data +
		openfile->current_x);

	assert(openfile->current_x < strlen(openfile->current->data));

	/* Let's get dangerous. */
	charmove(&openfile->current->data[openfile->current_x],
		&openfile->current->data[openfile->current_x +
		char_buf_len], line_len - char_buf_len + 1);

	null_at(&openfile->current->data, openfile->current_x +
		line_len - char_buf_len);
#ifndef NANO_SMALL
	if (openfile->mark_set && openfile->mark_begin ==
		openfile->current && openfile->current_x <
		openfile->mark_begin_x)
	    openfile->mark_begin_x -= char_buf_len;
#endif
	openfile->totsize--;
    } else if (openfile->current != openfile->filebot &&
	(openfile->current->next != openfile->filebot ||
	openfile->current->data[0] == '\0')) {
	/* We can delete the line before filebot only if it is blank: it
	 * becomes the new magicline then. */
	filestruct *foo = openfile->current->next;

	assert(openfile->current_x == strlen(openfile->current->data));

	/* If we're deleting at the end of a line, we need to call
	 * edit_refresh(). */
	if (openfile->current->data[openfile->current_x] == '\0')
	    do_refresh = TRUE;

	openfile->current->data = charealloc(openfile->current->data,
		openfile->current_x + strlen(foo->data) + 1);
	strcpy(openfile->current->data + openfile->current_x,
		foo->data);
#ifndef NANO_SMALL
	if (openfile->mark_set && openfile->mark_begin ==
		openfile->current->next) {
	    openfile->mark_begin = openfile->current;
	    openfile->mark_begin_x += openfile->current_x;
	}
#endif
	if (openfile->filebot == foo)
	    openfile->filebot = openfile->current;

	unlink_node(foo);
	delete_node(foo);
	renumber(openfile->current);
	openfile->totsize--;
#ifndef DISABLE_WRAPPING
	wrap_reset();
#endif
    } else
	return;

    set_modified();

#ifdef ENABLE_COLOR
    /* If color syntaxes are available and turned on, we need to call
     * edit_refresh(). */
    if (openfile->colorstrings != NULL && !ISSET(NO_COLOR_SYNTAX))
	do_refresh = TRUE;
#endif

    if (do_refresh)
	edit_refresh();
    else
	update_line(openfile->current, openfile->current_x);
}

void do_backspace(void)
{
    if (openfile->current != openfile->fileage ||
	openfile->current_x > 0) {
	do_left(FALSE);
	do_delete();
    }
}

void do_tab(void)
{
#ifndef NANO_SMALL
    if (ISSET(TABS_TO_SPACES)) {
	char *output;
	size_t output_len = 0, new_pww = openfile->placewewant;

	do {
	    new_pww++;
	    output_len++;
	} while (new_pww % tabsize != 0);

	output = charalloc(output_len + 1);

	charset(output, ' ', output_len);
	output[output_len] = '\0';

	do_output(output, output_len, TRUE);

	free(output);
    } else {
#endif
	do_output("\t", 1, TRUE);
#ifndef NANO_SMALL
    }
#endif
}

/* Someone hits Return *gasp!* */
void do_enter(void)
{
    filestruct *newnode = make_new_node(openfile->current);
    size_t extra = 0;

    assert(openfile->current != NULL && openfile->current->data != NULL);

#ifndef NANO_SMALL
    /* Do auto-indenting, like the neolithic Turbo Pascal editor. */
    if (ISSET(AUTOINDENT)) {
	/* If we are breaking the line in the indentation, the new
	 * indentation should have only current_x characters, and
	 * current_x should not change. */
	extra = indent_length(openfile->current->data);
	if (extra > openfile->current_x)
	    extra = openfile->current_x;
    }
#endif
    newnode->data = charalloc(strlen(openfile->current->data +
	openfile->current_x) + extra + 1);
    strcpy(&newnode->data[extra], openfile->current->data +
	openfile->current_x);
#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	strncpy(newnode->data, openfile->current->data, extra);
	openfile->totsize += mbstrlen(newnode->data);
    }
#endif
    null_at(&openfile->current->data, openfile->current_x);
#ifndef NANO_SMALL
    if (openfile->mark_set && openfile->current ==
	openfile->mark_begin && openfile->current_x <
	openfile->mark_begin_x) {
	openfile->mark_begin = newnode;
	openfile->mark_begin_x += extra - openfile->current_x;
    }
#endif
    openfile->current_x = extra;

    if (openfile->current == openfile->filebot)
	openfile->filebot = newnode;
    splice_node(openfile->current, newnode,
	openfile->current->next);

    renumber(openfile->current);
    openfile->current = newnode;

    edit_refresh();

    openfile->totsize++;
    set_modified();
    openfile->placewewant = xplustabs();
}

#ifndef NANO_SMALL
void cancel_command(int signal)
{
    if (kill(pid, SIGKILL) == -1)
	nperror("kill");
}

/* Return TRUE on success. */
bool execute_command(const char *command)
{
    int fd[2];
    FILE *f;
    struct sigaction oldaction, newaction;
	/* Original and temporary handlers for SIGINT. */
    bool sig_failed = FALSE;
	/* Did sigaction() fail without changing the signal handlers? */

    /* Make our pipes. */
    if (pipe(fd) == -1) {
	statusbar(_("Could not pipe"));
	return FALSE;
    }

    /* Fork a child. */
    if ((pid = fork()) == 0) {
	close(fd[0]);
	dup2(fd[1], fileno(stdout));
	dup2(fd[1], fileno(stderr));

	/* If execl() returns at all, there was an error. */
	execl("/bin/sh", "sh", "-c", command, NULL);
	exit(0);
    }

    /* Else continue as parent. */
    close(fd[1]);

    if (pid == -1) {
	close(fd[0]);
	statusbar(_("Could not fork"));
	return FALSE;
    }

    /* Before we start reading the forked command's output, we set
     * things up so that Ctrl-C will cancel the new process. */

    /* Enable interpretation of the special control keys so that we get
     * SIGINT when Ctrl-C is pressed. */
    enable_signals();

    if (sigaction(SIGINT, NULL, &newaction) == -1) {
	sig_failed = TRUE;
	nperror("sigaction");
    } else {
	newaction.sa_handler = cancel_command;
	if (sigaction(SIGINT, &newaction, &oldaction) == -1) {
	    sig_failed = TRUE;
	    nperror("sigaction");
	}
    }

    /* Note that now oldaction is the previous SIGINT signal handler,
     * to be restored later. */

    f = fdopen(fd[0], "rb");
    if (f == NULL)
	nperror("fdopen");

    read_file(f, "stdin");

    /* If multibuffer mode is on, we could be here in view mode.  If so,
     * don't set the modification flag. */
    if (!ISSET(VIEW_MODE))
	set_modified();

    if (wait(NULL) == -1)
	nperror("wait");

    if (!sig_failed && sigaction(SIGINT, &oldaction, NULL) == -1)
	nperror("sigaction");

    /* Disable interpretation of the special control keys so that we can
     * use Ctrl-C for other things. */
    disable_signals();

    return TRUE;
}
#endif /* !NANO_SMALL */

#ifndef DISABLE_WRAPPING
void wrap_reset(void)
{
    same_line_wrap = FALSE;
}

/* We wrap the given line.  Precondition: we assume the cursor has been
 * moved forward since the last typed character.  Return value: whether
 * we wrapped. */
bool do_wrap(filestruct *line)
{
    size_t line_len;
	/* The length of the line we wrap. */
    ssize_t wrap_loc;
	/* The index of line->data where we wrap. */
#ifndef NANO_SMALL
    const char *indent_string = NULL;
	/* Indentation to prepend to the new line. */
    size_t indent_len = 0;
	/* The length of indent_string. */
#endif
    const char *after_break;
	/* The text after the wrap point. */
    size_t after_break_len;
	/* The length of after_break. */
    bool wrapping = FALSE;
	/* Do we prepend to the next line? */
    const char *next_line = NULL;
	/* The next line, minus indentation. */
    size_t next_line_len = 0;
	/* The length of next_line. */
    char *new_line = NULL;
	/* The line we create. */
    size_t new_line_len = 0;
	/* The eventual length of new_line. */

    /* There are three steps.  First, we decide where to wrap.  Then, we
     * create the new wrap line.  Finally, we clean up. */

    /* Step 1, finding where to wrap.  We are going to add a new line
     * after a blank character.  In this step, we call break_line() to
     * get the location of the last blank we can break the line at, and
     * and set wrap_loc to the location of the character after it, so
     * that the blank is preserved at the end of the line.
     *
     * If there is no legal wrap point, or we reach the last character
     * of the line while trying to find one, we should return without
     * wrapping.  Note that if autoindent is turned on, we don't break
     * at the end of it! */

    assert(line != NULL && line->data != NULL);

    /* Save the length of the line. */
    line_len = strlen(line->data);

    /* Find the last blank where we can break the line. */
    wrap_loc = break_line(line->data, fill, FALSE);

    /* If we couldn't break the line, or we've reached the end of it, we
     * don't wrap. */
    if (wrap_loc == -1 || line->data[wrap_loc] == '\0')
	return FALSE;

    /* Otherwise, move forward to the character just after the blank. */
    wrap_loc += move_mbright(line->data + wrap_loc, 0);

    /* If we've reached the end of the line, we don't wrap. */
    if (line->data[wrap_loc] == '\0')
	return FALSE;

#ifndef NANO_SMALL
    /* If autoindent is turned on, and we're on the character just after
     * the indentation, we don't wrap. */
    if (ISSET(AUTOINDENT)) {
	/* Get the indentation of this line. */
	indent_string = line->data;
	indent_len = indent_length(indent_string);

	if (wrap_loc == indent_len)
	    return FALSE;
    }
#endif

    /* Step 2, making the new wrap line.  It will consist of indentation
     * followed by the text after the wrap point, optionally followed by
     * a space (if the text after the wrap point doesn't end in a blank)
     * and the text of the next line, if they can fit without
     * wrapping, the next line exists, and the same_line_wrap flag is
     * set. */

    /* after_break is the text that will be wrapped to the next line. */
    after_break = line->data + wrap_loc;
    after_break_len = line_len - wrap_loc;

    assert(strlen(after_break) == after_break_len);

    /* We prepend the wrapped text to the next line, if the
     * same_line_wrap flag is set, there is a next line, and prepending
     * would not make the line too long. */
    if (same_line_wrap && line->next != NULL) {
	const char *end = after_break + move_mbleft(after_break,
		after_break_len);

	/* If after_break doesn't end in a blank, make sure it ends in a
	 * space. */
	if (!is_blank_mbchar(end)) {
	    line_len++;
	    line->data = charealloc(line->data, line_len + 1);
	    line->data[line_len - 1] = ' ';
	    line->data[line_len] = '\0';
	    after_break = line->data + wrap_loc;
	    after_break_len++;
	    openfile->totsize++;
	}

	next_line = line->next->data;
	next_line_len = strlen(next_line);

	if (after_break_len + next_line_len <= fill) {
	    wrapping = TRUE;
	    new_line_len += next_line_len;
	}
    }

    /* new_line_len is now the length of the text that will be wrapped
     * to the next line, plus (if we're prepending to it) the length of
     * the text of the next line. */
    new_line_len += after_break_len;

#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	if (wrapping) {
	    /* If we're wrapping, the indentation will come from the
	     * next line. */
	    indent_string = next_line;
	    indent_len = indent_length(indent_string);
	    next_line += indent_len;
	} else {
	    /* Otherwise, it will come from this line, in which case
	     * we should increase new_line_len to make room for it. */
	    new_line_len += indent_len;
	    openfile->totsize += mbstrnlen(indent_string, indent_len);
	}
    }
#endif

    /* Now we allocate the new line and copy the text into it. */
    new_line = charalloc(new_line_len + 1);
    new_line[0] = '\0';

#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	/* Copy the indentation. */
	strncpy(new_line, indent_string, indent_len);
	new_line[indent_len] = '\0';
	new_line_len += indent_len;
    }
#endif

    /* Copy all the text after the wrap point of the current line. */
    strcat(new_line, after_break);

    /* Break the current line at the wrap point. */
    null_at(&line->data, wrap_loc);

    if (wrapping) {
	/* If we're wrapping, copy the text from the next line, minus
	 * the indentation that we already copied above. */
	strcat(new_line, next_line);

	free(line->next->data);
	line->next->data = new_line;
    } else {
	/* Otherwise, make a new line and copy the text after where we
	 * broke this line to the beginning of the new line. */
	splice_node(openfile->current, make_new_node(openfile->current),
		openfile->current->next);

	openfile->current->next->data = new_line;

	openfile->totsize++;
    }

    /* Step 3, clean up.  Reposition the cursor and mark, and do some
     * other sundry things. */

    /* Set the same_line_wrap flag, so that later wraps of this line
     * will be prepended to the next line. */
    same_line_wrap = TRUE;

    /* Each line knows its line number.  We recalculate these if we
     * inserted a new line. */
    if (!wrapping)
	renumber(line);

    /* If the cursor was after the break point, we must move it.  We
     * also clear the same_line_wrap flag in this case. */
    if (openfile->current_x > wrap_loc) {
	same_line_wrap = FALSE;
	openfile->current = openfile->current->next;
	openfile->current_x -= wrap_loc
#ifndef NANO_SMALL
		- indent_len
#endif
		;
	openfile->placewewant = xplustabs();
    }

#ifndef NANO_SMALL
    /* If the mark was on this line after the wrap point, we move it
     * down.  If it was on the next line and we wrapped onto that line,
     * we move it right. */
    if (openfile->mark_set) {
	if (openfile->mark_begin == line && openfile->mark_begin_x >
		wrap_loc) {
	    openfile->mark_begin = line->next;
	    openfile->mark_begin_x -= wrap_loc - indent_len + 1;
	} else if (wrapping && openfile->mark_begin == line->next)
	    openfile->mark_begin_x += after_break_len;
    }
#endif

    return TRUE;
}
#endif /* !DISABLE_WRAPPING */

#if !defined(DISABLE_HELP) || !defined(DISABLE_JUSTIFY) || !defined(DISABLE_WRAPPING)
/* We are trying to break a chunk off line.  We find the last blank such
 * that the display length to there is at most goal + 1.  If there is no
 * such blank, then we find the first blank.  We then take the last
 * blank in that group of blanks.  The terminating '\0' counts as a
 * blank, as does a '\n' if newline is TRUE. */
ssize_t break_line(const char *line, ssize_t goal, bool newline)
{
    ssize_t blank_loc = -1;
	/* Current tentative return value.  Index of the last blank we
	 * found with short enough display width.  */
    ssize_t cur_loc = 0;
	/* Current index in line. */
    int line_len;

    assert(line != NULL);

    while (*line != '\0' && goal >= 0) {
	size_t pos = 0;

	line_len = parse_mbchar(line, NULL, &pos);

	if (is_blank_mbchar(line) || (newline && *line == '\n')) {
	    blank_loc = cur_loc;

	    if (newline && *line == '\n')
		break;
	}

	goal -= pos;
	line += line_len;
	cur_loc += line_len;
    }

    if (goal >= 0)
	/* In fact, the whole line displays shorter than goal. */
	return cur_loc;

    if (blank_loc == -1) {
	/* No blank was found that was short enough. */
	bool found_blank = FALSE;

	while (*line != '\0') {
	    line_len = parse_mbchar(line, NULL, NULL);

	    if (is_blank_mbchar(line) || (newline && *line == '\n')) {
		if (!found_blank)
		    found_blank = TRUE;
	    } else if (found_blank)
		return cur_loc - line_len;

	    line += line_len;
	    cur_loc += line_len;
	}

	return -1;
    }

    /* Move to the last blank after blank_loc, if there is one. */
    line -= cur_loc;
    line += blank_loc;
    line_len = parse_mbchar(line, NULL, NULL);
    line += line_len;

    while (*line != '\0' && (is_blank_mbchar(line) ||
	(newline && *line == '\n'))) {
	line_len = parse_mbchar(line, NULL, NULL);

	line += line_len;
	blank_loc += line_len;
    }

    return blank_loc;
}
#endif /* !DISABLE_HELP || !DISABLE_JUSTIFY || !DISABLE_WRAPPING */

#if !defined(NANO_SMALL) || !defined(DISABLE_JUSTIFY)
/* The "indentation" of a line is the whitespace between the quote part
 * and the non-whitespace of the line. */
size_t indent_length(const char *line)
{
    size_t len = 0;
    char *blank_mb;
    int blank_mb_len;

    assert(line != NULL);

    blank_mb = charalloc(mb_cur_max());

    while (*line != '\0') {
	blank_mb_len = parse_mbchar(line, blank_mb, NULL);

	if (!is_blank_mbchar(blank_mb))
	    break;

	line += blank_mb_len;
	len += blank_mb_len;
    }

    free(blank_mb);

    return len;
}
#endif /* !NANO_SMALL || !DISABLE_JUSTIFY */

#ifndef DISABLE_JUSTIFY
/* justify_format() replaces blanks with spaces and multiple spaces by 1
 * (except it maintains up to 2 after a character in punct optionally
 * followed by a character in brackets, and removes all from the end).
 *
 * justify_format() might make paragraph->data shorter, and change the
 * actual pointer with null_at().
 *
 * justify_format() will not look at the first skip characters of
 * paragraph.  skip should be at most strlen(paragraph->data).  The
 * character at paragraph[skip + 1] must not be blank. */
void justify_format(filestruct *paragraph, size_t skip)
{
    char *end, *new_end, *new_paragraph_data;
    size_t shift = 0;
#ifndef NANO_SMALL
    size_t mark_shift = 0;
#endif

    /* These four asserts are assumptions about the input data. */
    assert(paragraph != NULL);
    assert(paragraph->data != NULL);
    assert(skip < strlen(paragraph->data));
    assert(!is_blank_mbchar(paragraph->data + skip));

    end = paragraph->data + skip;
    new_paragraph_data = charalloc(strlen(paragraph->data) + 1);
    strncpy(new_paragraph_data, paragraph->data, skip);
    new_end = new_paragraph_data + skip;

    while (*end != '\0') {
	int end_len;

	/* If this character is blank, make sure that it's a space with
	 * no blanks after it. */
	if (is_blank_mbchar(end)) {
	    end_len = parse_mbchar(end, NULL, NULL);

	    *new_end = ' ';
	    new_end++;
	    end += end_len;

	    while (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL);

		end += end_len;
		shift += end_len;

#ifndef NANO_SMALL
		/* Keep track of the change in the current line. */
		if (openfile->mark_set && openfile->mark_begin ==
			paragraph && openfile->mark_begin_x >= end -
			paragraph->data)
		    mark_shift += end_len;
#endif
	    }
	/* If this character is punctuation optionally followed by a
	 * bracket and then followed by blanks, make sure there are no
	 * more than two blanks after it, and make sure that the blanks
	 * are spaces. */
	} else if (mbstrchr(punct, end) != NULL) {
	    end_len = parse_mbchar(end, NULL, NULL);

	    while (end_len > 0) {
		*new_end = *end;
		new_end++;
		end++;
		end_len--;
	    }

	    if (*end != '\0' && mbstrchr(brackets, end) != NULL) {
		end_len = parse_mbchar(end, NULL, NULL);

		while (end_len > 0) {
		    *new_end = *end;
		    new_end++;
		    end++;
		    end_len--;
		}
	    }

	    if (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL);

		*new_end = ' ';
		new_end++;
		end += end_len;
	    }

	    if (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL);

		*new_end = ' ';
		new_end++;
		end += end_len;
	    }

	    while (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL);

		end += end_len;
		shift += end_len;

#ifndef NANO_SMALL
		/* Keep track of the change in the current line. */
		if (openfile->mark_set && openfile->mark_begin ==
			paragraph && openfile->mark_begin_x >= end -
			paragraph->data)
		    mark_shift += end_len;
#endif
	    }
	/* If this character is neither blank nor punctuation, leave it
	 * alone. */
	} else {
	    end_len = parse_mbchar(end, NULL, NULL);

	    while (end_len > 0) {
		*new_end = *end;
		new_end++;
		end++;
		end_len--;
	    }
	}
    }

    assert(*end == '\0');

    *new_end = *end;

    /* Make sure that there are no spaces at the end of the line. */
    while (new_end > new_paragraph_data + skip &&
	*(new_end - 1) == ' ') {
	new_end--;
	shift++;
    }

    if (shift > 0) {
	openfile->totsize -= shift;
	null_at(&new_paragraph_data, new_end - new_paragraph_data);
	free(paragraph->data);
	paragraph->data = new_paragraph_data;

#ifndef NANO_SMALL
	/* Adjust the mark coordinates to compensate for the change in
	 * the current line. */
	if (openfile->mark_set && openfile->mark_begin == paragraph) {
	    openfile->mark_begin_x -= mark_shift;
	    if (openfile->mark_begin_x > new_end - new_paragraph_data)
		openfile->mark_begin_x = new_end - new_paragraph_data;
	}
#endif
    } else
	free(new_paragraph_data);
}

/* The "quote part" of a line is the largest initial substring matching
 * the quote string.  This function returns the length of the quote part
 * of the given line.
 *
 * Note that if !HAVE_REGEX_H then we match concatenated copies of
 * quotestr. */
size_t quote_length(const char *line)
{
#ifdef HAVE_REGEX_H
    regmatch_t matches;
    int rc = regexec(&quotereg, line, 1, &matches, 0);

    if (rc == REG_NOMATCH || matches.rm_so == (regoff_t)-1)
	return 0;
    /* matches.rm_so should be 0, since the quote string should start
     * with the caret ^. */
    return matches.rm_eo;
#else	/* !HAVE_REGEX_H */
    size_t qdepth = 0;

    /* Compute quote depth level. */
    while (strncmp(line + qdepth, quotestr, quotelen) == 0)
	qdepth += quotelen;
    return qdepth;
#endif	/* !HAVE_REGEX_H */
}

/* a_line and b_line are lines of text.  The quotation part of a_line is
 * the first a_quote characters.  Check that the quotation part of
 * b_line is the same. */
bool quotes_match(const char *a_line, size_t a_quote, const char
	*b_line)
{
    /* Here is the assumption about a_quote. */
    assert(a_quote == quote_length(a_line));

    return (a_quote == quote_length(b_line) &&
	strncmp(a_line, b_line, a_quote) == 0);
}

/* We assume a_line and b_line have no quote part.  Then, we return
 * whether b_line could follow a_line in a paragraph. */
bool indents_match(const char *a_line, size_t a_indent, const char
	*b_line, size_t b_indent)
{
    assert(a_indent == indent_length(a_line));
    assert(b_indent == indent_length(b_line));

    return (b_indent <= a_indent &&
	strncmp(a_line, b_line, b_indent) == 0);
}

/* Is foo the beginning of a paragraph?
 *
 *   A line of text consists of a "quote part", followed by an
 *   "indentation part", followed by text.  The functions quote_length()
 *   and indent_length() calculate these parts.
 *
 *   A line is "part of a paragraph" if it has a part not in the quote
 *   part or the indentation.
 *
 *   A line is "the beginning of a paragraph" if it is part of a
 *   paragraph and
 *	1) it is the top line of the file, or
 *	2) the line above it is not part of a paragraph, or
 *	3) the line above it does not have precisely the same quote
 *	   part, or
 *	4) the indentation of this line is not an initial substring of
 *	   the indentation of the previous line, or
 *	5) this line has no quote part and some indentation, and
 *	   autoindent isn't turned on.
 *   The reason for number 5) is that if autoindent isn't turned on,
 *   then an indented line is expected to start a paragraph, as in
 *   books.  Thus, nano can justify an indented paragraph only if
 *   autoindent is turned on. */
bool begpar(const filestruct *const foo)
{
    size_t quote_len;
    size_t indent_len;
    size_t temp_id_len;

    /* Case 1). */
    if (foo->prev == NULL)
	return TRUE;

    quote_len = quote_length(foo->data);
    indent_len = indent_length(foo->data + quote_len);

    /* Not part of a paragraph. */
    if (foo->data[quote_len + indent_len] == '\0')
	return FALSE;

    /* Case 3). */
    if (!quotes_match(foo->data, quote_len, foo->prev->data))
	return TRUE;

    temp_id_len = indent_length(foo->prev->data + quote_len);

    /* Case 2) or 5) or 4). */
    if (foo->prev->data[quote_len + temp_id_len] == '\0' ||
	(quote_len == 0 && indent_len > 0
#ifndef NANO_SMALL
	&& !ISSET(AUTOINDENT)
#endif
	) || !indents_match(foo->prev->data + quote_len, temp_id_len,
	foo->data + quote_len, indent_len))
	return TRUE;

    return FALSE;
}

/* Is foo inside a paragraph? */
bool inpar(const filestruct *const foo)
{
    size_t quote_len;

    if (foo == NULL)
	return FALSE;

    quote_len = quote_length(foo->data);

    return foo->data[quote_len + indent_length(foo->data +
	quote_len)] != '\0';
}

/* Put the next par_len lines, starting with first_line, into the
 * justify buffer, leaving copies of those lines in place.  Assume there
 * are enough lines after first_line.  Return the new copy of
 * first_line. */
filestruct *backup_lines(filestruct *first_line, size_t par_len, size_t
	quote_len)
{
    filestruct *top = first_line;
	/* The top of the paragraph we're backing up. */
    filestruct *bot = first_line;
	/* The bottom of the paragraph we're backing up. */
    size_t i;
	/* Generic loop variable. */
    size_t current_x_save = openfile->current_x;
    ssize_t fl_lineno_save = first_line->lineno;
    ssize_t edittop_lineno_save = openfile->edittop->lineno;
    ssize_t current_lineno_save = openfile->current->lineno;
#ifndef NANO_SMALL
    bool old_mark_set = openfile->mark_set;
    ssize_t mb_lineno_save = 0;
    size_t mark_begin_x_save = 0;

    if (old_mark_set) {
	mb_lineno_save = openfile->mark_begin->lineno;
	mark_begin_x_save = openfile->mark_begin_x;
    }
#endif

    /* Move bot down par_len lines to the newline after the last line of
     * the paragraph. */
    for (i = par_len; i > 0; i--)
	bot = bot->next;

    /* Move the paragraph from the current buffer's filestruct to the
     * justify buffer. */
    move_to_filestruct(&jusbuffer, &jusbottom, top, 0, bot, 0);

    /* Copy the paragraph back to the current buffer's filestruct from
     * the justify buffer. */
    copy_from_filestruct(jusbuffer, jusbottom);

    /* Move upward from the last line of the paragraph to the first
     * line, putting first_line, edittop, current, and mark_begin at the
     * same lines in the copied paragraph that they had in the original
     * paragraph. */
    top = openfile->current->prev;
    for (i = par_len; i > 0; i--) {
	if (top->lineno == fl_lineno_save)
	    first_line = top;
	if (top->lineno == edittop_lineno_save)
	    openfile->edittop = top;
	if (top->lineno == current_lineno_save)
	    openfile->current = top;
#ifndef NANO_SMALL
	if (old_mark_set && top->lineno == mb_lineno_save) {
	    openfile->mark_begin = top;
	    openfile->mark_begin_x = mark_begin_x_save;
	}
#endif
	top = top->prev;
    }

    /* Put current_x at the same place in the copied paragraph that it
     * had in the original paragraph. */
    openfile->current_x = current_x_save;

    set_modified();

    return first_line;
}

/* Find the beginning of the current paragraph if we're in one, or the
 * beginning of the next paragraph if we're not.  Afterwards, save the
 * quote length and paragraph length in *quote and *par.  Return TRUE if
 * we found a paragraph, or FALSE if there was an error or we didn't
 * find a paragraph.
 *
 * See the comment at begpar() for more about when a line is the
 * beginning of a paragraph. */
bool find_paragraph(size_t *const quote, size_t *const par)
{
    size_t quote_len;
	/* Length of the initial quotation of the paragraph we search
	 * for. */
    size_t par_len;
	/* Number of lines in the paragraph we search for. */
    filestruct *current_save;
	/* The line at the beginning of the paragraph we search for. */
    ssize_t current_y_save;
	/* The y-coordinate at the beginning of the paragraph we search
	 * for. */

#ifdef HAVE_REGEX_H
    if (quoterc != 0) {
	statusbar(_("Bad quote string %s: %s"), quotestr, quoteerr);
	return FALSE;
    }
#endif

    assert(openfile->current != NULL);

    /* Move back to the beginning of the current line. */
    openfile->current_x = 0;
    openfile->placewewant = 0;

    /* Find the first line of the current or next paragraph.  First, if
     * the current line isn't in a paragraph, move forward to the line
     * after the last line of the next paragraph.  If we end up on the
     * same line, or the line before that isn't in a paragraph, it means
     * that there aren't any paragraphs left, so get out.  Otherwise,
     * move back to the last line of the paragraph.  If the current line
     * is in a paragraph and it isn't the first line of that paragraph,
     * move back to the first line. */
    if (!inpar(openfile->current)) {
	current_save = openfile->current;

	do_para_end(FALSE);
	if (openfile->current == current_save ||
		!inpar(openfile->current->prev))
	    return FALSE;
	if (openfile->current->prev != NULL)
	    openfile->current = openfile->current->prev;
    }
    if (!begpar(openfile->current))
	do_para_begin(FALSE);

    /* Now current is the first line of the paragraph.  Set quote_len to
     * the quotation length of that line, and set par_len to the number
     * of lines in this paragraph. */
    quote_len = quote_length(openfile->current->data);
    current_save = openfile->current;
    current_y_save = openfile->current_y;
    do_para_end(FALSE);
    par_len = openfile->current->lineno - current_save->lineno;
    openfile->current = current_save;
    openfile->current_y = current_y_save;

    /* Save the values of quote_len and par_len. */
    assert(quote != NULL && par != NULL);

    *quote = quote_len;
    *par = par_len;

    return TRUE;
}

/* If full_justify is TRUE, justify the entire file.  Otherwise, justify
 * the current paragraph. */
void do_justify(bool full_justify)
{
    filestruct *first_par_line = NULL;
	/* Will be the first line of the justified paragraph.  For
	 * restoring after unjustify. */
    filestruct *last_par_line;
	/* Will be the line containing the newline after the last line
	 * of the justified paragraph.  Also for restoring after
	 * unjustify. */

    /* We save these variables to be restored if the user
     * unjustifies. */
    size_t current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    ssize_t current_y_save = openfile->current_y;
    bool modified_save = openfile->modified;
    size_t totsize_save = openfile->totsize;
    filestruct *edittop_save = openfile->edittop;
    filestruct *current_save = openfile->current;
#ifndef NANO_SMALL
    filestruct *mark_begin_save = openfile->mark_begin;
    size_t mark_begin_x_save = openfile->mark_begin_x;
#endif
    int kbinput;
    bool meta_key, func_key, s_or_t, ran_func, finished;

    /* If we're justifying the entire file, start at the beginning. */
    if (full_justify)
	openfile->current = openfile->fileage;

    last_par_line = openfile->current;

    while (TRUE) {
	size_t i;
	    /* Generic loop variable. */
	size_t quote_len;
	    /* Length of the initial quotation of the paragraph we
	     * justify. */
	size_t indent_len;
	    /* Length of the initial indentation of the paragraph we
	     * justify. */
	size_t par_len;
	    /* Number of lines in the paragraph we justify. */
	ssize_t break_pos;
	    /* Where we will break lines. */
	char *indent_string;
	    /* The first indentation that doesn't match the initial
	     * indentation of the paragraph we justify.  This is put at
	     * the beginning of every line broken off the first
	     * justified line of the paragraph.  (Note that this works
	     * because a paragraph can only contain two indentations at
	     * most: the initial one, and a different one starting on a
	     * line after the first.  See the comment at begpar() for
	     * more about when a line is part of a paragraph.) */

	/* Find the first line of the paragraph to be justified.  That
	 * is the start of this paragraph if we're in one, or the start
	 * of the next otherwise.  Save the quote length and paragraph
	 * length (number of lines).  Don't refresh the screen yet,
	 * since we'll do that after we justify.
	 *
	 * If the search failed, we do one of two things.  If we're
	 * justifying the whole file, we've found at least one
	 * paragraph, and the search didn't leave us on the last line of
	 * the file, it means that we should justify all the way to the
	 * last line of the file, so set the last line of the text to be
	 * justified to the last line of the file and break out of the
	 * loop.  Otherwise, it means that there are no paragraph(s) to
	 * justify, so refresh the screen and get out. */
	if (!find_paragraph(&quote_len, &par_len)) {
	    if (full_justify && first_par_line != NULL &&
		first_par_line != openfile->filebot) {
		last_par_line = openfile->filebot;
		break;
	    } else {
		edit_refresh();
		return;
	    }
	}

	/* If we haven't already done it, copy the original paragraph(s)
	 * to the justify buffer. */
	if (first_par_line == NULL)
	    first_par_line = backup_lines(openfile->current,
		full_justify ? openfile->filebot->lineno -
		openfile->current->lineno : par_len, quote_len);

	/* Initialize indent_string to a blank string. */
	indent_string = mallocstrcpy(NULL, "");

	/* Find the first indentation in the paragraph that doesn't
	 * match the indentation of the first line, and save it�in
	 * indent_string.  If all the indentations are the same, save
	 * the indentation of the first line in indent_string. */
	{
	    const filestruct *indent_line = openfile->current;
	    bool past_first_line = FALSE;

	    for (i = 0; i < par_len; i++) {
		indent_len = quote_len +
			indent_length(indent_line->data + quote_len);

		if (indent_len != strlen(indent_string)) {
		    indent_string = mallocstrncpy(indent_string,
			indent_line->data, indent_len + 1);
		    indent_string[indent_len] = '\0';

		    if (past_first_line)
			break;
		}

		if (indent_line == openfile->current)
		    past_first_line = TRUE;

		indent_line = indent_line->next;
	    }
	}

	/* Now tack all the lines of the paragraph together, skipping
	 * the quoting and indentation on all lines after the first. */
	for (i = 0; i < par_len - 1; i++) {
	    filestruct *next_line = openfile->current->next;
	    size_t line_len = strlen(openfile->current->data);
	    size_t next_line_len =
		strlen(openfile->current->next->data);

	    indent_len = quote_len +
		indent_length(openfile->current->next->data +
		quote_len);

	    next_line_len -= indent_len;
	    openfile->totsize -= indent_len;

	    /* We're just about to tack the next line onto this one.  If
	     * this line isn't empty, make sure it ends in a space. */
	    if (line_len > 0 &&
		openfile->current->data[line_len - 1] != ' ') {
		line_len++;
		openfile->current->data =
			charealloc(openfile->current->data,
			line_len + 1);
		openfile->current->data[line_len - 1] = ' ';
		openfile->current->data[line_len] = '\0';
		openfile->totsize++;
	    }

	    openfile->current->data =
		charealloc(openfile->current->data, line_len +
		next_line_len + 1);
	    strcat(openfile->current->data, next_line->data +
		indent_len);

	    /* Don't destroy edittop! */
	    if (openfile->edittop == next_line)
		openfile->edittop = openfile->current;

#ifndef NANO_SMALL
	    /* Adjust the mark coordinates to compensate for the change
	     * in the next line. */
	    if (openfile->mark_set && openfile->mark_begin ==
		next_line) {
		openfile->mark_begin = openfile->current;
		openfile->mark_begin_x += line_len - indent_len;
	    }
#endif

	    unlink_node(next_line);
	    delete_node(next_line);

	    /* If we've removed the next line, we need to go through
	     * this line again. */
	    i--;

	    par_len--;
	    openfile->totsize--;
	}

	/* Call justify_format() on the paragraph, which will remove
	 * excess spaces from it and change all blank characters to
	 * spaces. */
	justify_format(openfile->current, quote_len +
		indent_length(openfile->current->data + quote_len));

	while (par_len > 0 && strlenpt(openfile->current->data) >
		fill) {
	    size_t line_len = strlen(openfile->current->data);

	    indent_len = strlen(indent_string);

	    /* If this line is too long, try to wrap it to the next line
	     * to make it short enough. */
	    break_pos = break_line(openfile->current->data + indent_len,
		fill - strnlenpt(openfile->current->data, indent_len),
		FALSE);

	    /* We can't break the line, or don't need to, so get out. */
	    if (break_pos == -1 || break_pos + indent_len == line_len)
		break;

	    /* Move forward to the character after the indentation and
	     * just after the space. */
	    break_pos += indent_len + 1;

	    assert(break_pos <= line_len);

	    /* Make a new line, and copy the text after where we're
	     * going to break this line to the beginning of the new
	     * line. */
	    splice_node(openfile->current,
		make_new_node(openfile->current),
		openfile->current->next);

	    /* If this paragraph is non-quoted, and autoindent isn't
	     * turned on, set the indentation length to zero so that the
	     * indentation is treated as part of the line. */
	    if (quote_len == 0
#ifndef NANO_SMALL
		&& !ISSET(AUTOINDENT)
#endif
		)
		indent_len = 0;

	    /* Copy the text after where we're going to break the
	     * current line to the next line. */
	    openfile->current->next->data = charalloc(indent_len + 1 +
		line_len - break_pos);
	    strncpy(openfile->current->next->data, indent_string,
		indent_len);
	    strcpy(openfile->current->next->data + indent_len,
		openfile->current->data + break_pos);

	    par_len++;
	    openfile->totsize += indent_len + 1;

#ifndef NANO_SMALL
	    /* Adjust the mark coordinates to compensate for the change
	     * in the current line. */
	    if (openfile->mark_set && openfile->mark_begin ==
		openfile->current && openfile->mark_begin_x >
		break_pos) {
		openfile->mark_begin = openfile->current->next;
		openfile->mark_begin_x -= break_pos - indent_len;
	    }
#endif

	    /* Break the current line. */
	    null_at(&openfile->current->data, break_pos);

	    /* Go to the next line. */
	    par_len--;
	    openfile->current_y++;
	    openfile->current = openfile->current->next;
	}

	/* We're done breaking lines, so we don't need indent_string
	 * anymore. */
	free(indent_string);

	/* Go to the next line, the line after the last line of the
	 * paragraph. */
	openfile->current_y++;
	openfile->current = openfile->current->next;

	/* We've just justified a paragraph. If we're not justifying the
	 * entire file, break out of the loop.  Otherwise, continue the
	 * loop so that we justify all the paragraphs in the file. */
	if (!full_justify)
	    break;
    }

    /* We are now done justifying the paragraph or the file, so clean
     * up.  totsize, and current_y have been maintained above.  Set
     * last_par_line to the new end of the paragraph, update fileage,
     * and renumber, since edit_refresh() needs the line numbers to be
     * right (but only do the last two if we actually justified
     * something). */
    last_par_line = openfile->current;
    if (first_par_line != NULL) {
	if (first_par_line->prev == NULL)
	    openfile->fileage = first_par_line;
	renumber(first_par_line);
    }

    edit_refresh();

    statusbar(_("Can now UnJustify!"));

    /* If constant cursor position display is on, make sure the current
     * cursor position will be properly displayed on the statusbar. */
    if (ISSET(CONST_UPDATE))
	do_cursorpos(TRUE);

    /* Display the shortcut list with UnJustify. */
    shortcut_init(TRUE);
    display_main_list();

    /* Now get a keystroke and see if it's unjustify.  If not, put back
     * the keystroke and return. */
    kbinput = do_input(&meta_key, &func_key, &s_or_t, &ran_func,
	&finished, FALSE);

    if (!meta_key && !func_key && s_or_t &&
	kbinput == NANO_UNJUSTIFY_KEY) {
	/* Restore the justify we just did (ungrateful user!). */
	openfile->current = current_save;
	openfile->current_x = current_x_save;
	openfile->placewewant = pww_save;
	openfile->current_y = current_y_save;
	openfile->edittop = edittop_save;

	/* Splice the justify buffer back into the file, but only if we
	 * actually justified something. */
	if (first_par_line != NULL) {
	    filestruct *top_save;

	    /* Partition the filestruct so that it contains only the
	     * text of the justified paragraph. */
	    filepart = partition_filestruct(first_par_line, 0,
		last_par_line, 0);

	    /* Remove the text of the justified paragraph, and
	     * put the text in the justify buffer in its place. */
	    free_filestruct(openfile->fileage);
	    openfile->fileage = jusbuffer;
	    openfile->filebot = jusbottom;

	    top_save = openfile->fileage;

	    /* Unpartition the filestruct so that it contains all the
	     * text again.  Note that the justified paragraph has been
	     * replaced with the unjustified paragraph. */
	    unpartition_filestruct(&filepart);

	     /* Renumber starting with the beginning line of the old
	      * partition. */
	    renumber(top_save);

	    /* Restore variables from before the justify. */
	    openfile->totsize = totsize_save;
#ifndef NANO_SMALL
	    if (openfile->mark_set) {
		openfile->mark_begin = mark_begin_save;
		openfile->mark_begin_x = mark_begin_x_save;
	    }
#endif
	    openfile->modified = modified_save;

	    /* Clear the justify buffer. */
	    jusbuffer = NULL;

	    if (!openfile->modified)
		titlebar(NULL);
	    edit_refresh();
	}
    } else {
	unget_kbinput(kbinput, meta_key, func_key);

	/* Blow away the text in the justify buffer. */
	free_filestruct(jusbuffer);
	jusbuffer = NULL;
    }

    blank_statusbar();

    /* Display the shortcut list with UnCut. */
    shortcut_init(FALSE);
    display_main_list();
}

void do_justify_void(void)
{
    do_justify(FALSE);
}

void do_full_justify(void)
{
    do_justify(TRUE);
}
#endif /* !DISABLE_JUSTIFY */

#ifndef DISABLE_SPELLER
/* A word is misspelled in the file.  Let the user replace it.  We
 * return FALSE if the user cancels. */
bool do_int_spell_fix(const char *word)
{
    char *save_search, *save_replace;
    size_t match_len, current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    filestruct *edittop_save = openfile->edittop;
    filestruct *current_save = openfile->current;
	/* Save where we are. */
    bool canceled = FALSE;
	/* The return value. */
    bool case_sens_set = ISSET(CASE_SENSITIVE);
#ifndef NANO_SMALL
    bool backwards_search_set = ISSET(BACKWARDS_SEARCH);
#endif
#ifdef HAVE_REGEX_H
    bool regexp_set = ISSET(USE_REGEXP);
#endif
#ifndef NANO_SMALL
    bool old_mark_set = openfile->mark_set;
    bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
    bool right_side_up = FALSE;
	/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
	 * FALSE if (current, current_x) is. */
    filestruct *top, *bot;
    size_t top_x, bot_x;
#endif

    /* Make sure spell-check is case sensitive. */
    SET(CASE_SENSITIVE);

#ifndef NANO_SMALL
    /* Make sure spell-check goes forward only. */
    UNSET(BACKWARDS_SEARCH);
#endif
#ifdef HAVE_REGEX_H
    /* Make sure spell-check doesn't use regular expressions. */
    UNSET(USE_REGEXP);
#endif

    /* Save the current search/replace strings. */
    search_init_globals();
    save_search = last_search;
    save_replace = last_replace;

    /* Set the search/replace strings to the misspelled word. */
    last_search = mallocstrcpy(NULL, word);
    last_replace = mallocstrcpy(NULL, word);

#ifndef NANO_SMALL
    if (old_mark_set) {
	/* If the mark is on, partition the filestruct so that it
	 * contains only the marked text, keep track of whether the text
	 * will have a magicline added when we're done correcting
	 * misspelled words, and turn the mark off. */
	mark_order((const filestruct **)&top, &top_x,
	    (const filestruct **)&bot, &bot_x, &right_side_up);
	filepart = partition_filestruct(top, top_x, bot, bot_x);
	added_magicline = (openfile->filebot->data[0] != '\0');
	openfile->mark_set = FALSE;
    }
#endif

    /* Start from the top of the file. */
    openfile->edittop = openfile->fileage;
    openfile->current = openfile->fileage;
    openfile->current_x = (size_t)-1;
    openfile->placewewant = 0;

    /* Find the first whole-word occurrence of word. */
    findnextstr_wrap_reset();
    while (findnextstr(TRUE, TRUE, FALSE, openfile->fileage, 0, word,
	&match_len)) {
	if (is_whole_word(openfile->current_x, openfile->current->data,
		word)) {
	    size_t xpt = xplustabs();
	    char *exp_word = display_string(openfile->current->data,
		xpt, strnlenpt(openfile->current->data,
		openfile->current_x + match_len) - xpt, FALSE);

	    edit_refresh();

	    do_replace_highlight(TRUE, exp_word);

	    /* Allow all instances of the word to be corrected. */
	    canceled = (statusq(FALSE, spell_list, word,
#ifndef NANO_SMALL
			NULL,
#endif
			 _("Edit a replacement")) == -1);

	    do_replace_highlight(FALSE, exp_word);

	    free(exp_word);

	    if (!canceled && strcmp(word, answer) != 0) {
		openfile->current_x--;
		do_replace_loop(word, openfile->current,
			&openfile->current_x, TRUE, &canceled);
	    }

	    break;
	}
    }

#ifndef NANO_SMALL
    if (old_mark_set) {
	/* If the mark was on and we added a magicline, remove it
	 * now. */
	if (added_magicline)
	    remove_magicline();

	/* Put the beginning and the end of the mark at the beginning
	 * and the end of the spell-checked text. */
	if (openfile->fileage == openfile->filebot)
	    bot_x += top_x;
	if (right_side_up) {
	    openfile->mark_begin_x = top_x;
	    current_x_save = bot_x;
	} else {
	    current_x_save = top_x;
	    openfile->mark_begin_x = bot_x;
	}

	/* Unpartition the filestruct so that it contains all the text
	 * again, and turn the mark back on. */
	unpartition_filestruct(&filepart);
	openfile->mark_set = TRUE;
    }
#endif

    /* Restore the search/replace strings. */
    free(last_search);
    last_search = save_search;
    free(last_replace);
    last_replace = save_replace;

    /* Restore where we were. */
    openfile->edittop = edittop_save;
    openfile->current = current_save;
    openfile->current_x = current_x_save;
    openfile->placewewant = pww_save;

    /* Restore case sensitivity setting. */
    if (!case_sens_set)
	UNSET(CASE_SENSITIVE);

#ifndef NANO_SMALL
    /* Restore search/replace direction. */
    if (backwards_search_set)
	SET(BACKWARDS_SEARCH);
#endif
#ifdef HAVE_REGEX_H
    /* Restore regular expression usage setting. */
    if (regexp_set)
	SET(USE_REGEXP);
#endif

    return !canceled;
}

/* Integrated spell checking using the spell program, filtered through
 * the sort and uniq programs.  Return NULL for normal termination,
 * and the error string otherwise. */
const char *do_int_speller(const char *tempfile_name)
{
    char *read_buff, *read_buff_ptr, *read_buff_word;
    size_t pipe_buff_size, read_buff_size, read_buff_read, bytesread;
    int spell_fd[2], sort_fd[2], uniq_fd[2], tempfile_fd = -1;
    pid_t pid_spell, pid_sort, pid_uniq;
    int spell_status, sort_status, uniq_status;

    /* Create all three pipes up front. */
    if (pipe(spell_fd) == -1 || pipe(sort_fd) == -1 ||
	pipe(uniq_fd) == -1)
	return _("Could not create pipe");

    statusbar(_("Creating misspelled word list, please wait..."));

    /* A new process to run spell in. */
    if ((pid_spell = fork()) == 0) {
	/* Child continues (i.e, future spell process). */
	close(spell_fd[0]);

	/* Replace the standard input with the temp file. */
	if ((tempfile_fd = open(tempfile_name, O_RDONLY)) == -1)
	    goto close_pipes_and_exit;

	if (dup2(tempfile_fd, STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(tempfile_fd);

	/* Send spell's standard output to the pipe. */
	if (dup2(spell_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(spell_fd[1]);

	/* Start the spell program; we are using PATH. */
	execlp("spell", "spell", NULL);

	/* This should not be reached if spell is found. */
	exit(1);
    }

    /* Parent continues here. */
    close(spell_fd[1]);

    /* A new process to run sort in. */
    if ((pid_sort = fork()) == 0) {
	/* Child continues (i.e, future spell process).  Replace the
	 * standard input with the standard output of the old pipe. */
	if (dup2(spell_fd[0], STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(spell_fd[0]);

	/* Send sort's standard output to the new pipe. */
	if (dup2(sort_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(sort_fd[1]);

	/* Start the sort program.  Use -f to remove mixed case.  If
	 * this isn't portable, let me know. */
	execlp("sort", "sort", "-f", NULL);

	/* This should not be reached if sort is found. */
	exit(1);
    }

    close(spell_fd[0]);
    close(sort_fd[1]);

    /* A new process to run uniq in. */
    if ((pid_uniq = fork()) == 0) {
	/* Child continues (i.e, future uniq process).  Replace the
	 * standard input with the standard output of the old pipe. */
	if (dup2(sort_fd[0], STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(sort_fd[0]);

	/* Send uniq's standard output to the new pipe. */
	if (dup2(uniq_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(uniq_fd[1]);

	/* Start the uniq program; we are using PATH. */
	execlp("uniq", "uniq", NULL);

	/* This should not be reached if uniq is found. */
	exit(1);
    }

    close(sort_fd[0]);
    close(uniq_fd[1]);

    /* The child process was not forked successfully. */
    if (pid_spell < 0 || pid_sort < 0 || pid_uniq < 0) {
	close(uniq_fd[0]);
	return _("Could not fork");
    }

    /* Get the system pipe buffer size. */
    if ((pipe_buff_size = fpathconf(uniq_fd[0], _PC_PIPE_BUF)) < 1) {
	close(uniq_fd[0]);
	return _("Could not get size of pipe buffer");
    }

    /* Read in the returned spelling errors. */
    read_buff_read = 0;
    read_buff_size = pipe_buff_size + 1;
    read_buff = read_buff_ptr = charalloc(read_buff_size);

    while ((bytesread = read(uniq_fd[0], read_buff_ptr,
	pipe_buff_size)) > 0) {
	read_buff_read += bytesread;
	read_buff_size += pipe_buff_size;
	read_buff = read_buff_ptr = charealloc(read_buff,
		read_buff_size);
	read_buff_ptr += read_buff_read;
    }

    *read_buff_ptr = '\0';
    close(uniq_fd[0]);

    /* Process the spelling errors. */
    read_buff_word = read_buff_ptr = read_buff;

    while (*read_buff_ptr != '\0') {
	if ((*read_buff_ptr == '\r') || (*read_buff_ptr == '\n')) {
	    *read_buff_ptr = '\0';
	    if (read_buff_word != read_buff_ptr) {
		if (!do_int_spell_fix(read_buff_word)) {
		    read_buff_word = read_buff_ptr;
		    break;
		}
	    }
	    read_buff_word = read_buff_ptr + 1;
	}
	read_buff_ptr++;
    }

    /* Special case: the last word doesn't end with '\r' or '\n'. */
    if (read_buff_word != read_buff_ptr)
	do_int_spell_fix(read_buff_word);

    free(read_buff);
    replace_abort();
    edit_refresh();

    /* Process the end of the spell process. */
    waitpid(pid_spell, &spell_status, 0);
    waitpid(pid_sort, &sort_status, 0);
    waitpid(pid_uniq, &uniq_status, 0);

    if (WIFEXITED(spell_status) == 0 || WEXITSTATUS(spell_status))
	return _("Error invoking \"spell\"");

    if (WIFEXITED(sort_status)  == 0 || WEXITSTATUS(sort_status))
	return _("Error invoking \"sort -f\"");

    if (WIFEXITED(uniq_status) == 0 || WEXITSTATUS(uniq_status))
	return _("Error invoking \"uniq\"");

    /* Otherwise... */
    return NULL;

  close_pipes_and_exit:
    /* Don't leak any handles. */
    close(tempfile_fd);
    close(spell_fd[0]);
    close(spell_fd[1]);
    close(sort_fd[0]);
    close(sort_fd[1]);
    close(uniq_fd[0]);
    close(uniq_fd[1]);
    exit(1);
}

/* External spell checking.  Return value: NULL for normal termination,
 * otherwise the error string. */
const char *do_alt_speller(char *tempfile_name)
{
    int alt_spell_status;
    size_t current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    ssize_t current_y_save = openfile->current_y;
    ssize_t lineno_save = openfile->current->lineno;
    pid_t pid_spell;
    char *ptr;
    static int arglen = 3;
    static char **spellargs = NULL;
    FILE *f;
#ifndef NANO_SMALL
    bool old_mark_set = openfile->mark_set;
    bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
    bool right_side_up = FALSE;
	/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
	 * FALSE if (current, current_x) is. */
    filestruct *top, *bot;
    size_t top_x, bot_x;
    ssize_t mb_lineno_save = 0;
	/* We're going to close the current file, and open the output of
	 * the alternate spell command.  The line that mark_begin points
	 * to will be freed, so we save the line number and restore it
	 * afterwards. */
    size_t totsize_save = openfile->totsize;
	/* Our saved value of totsize, used when we spell-check a marked
	 * selection. */

    if (old_mark_set) {
	/* If the mark is on, save the number of the line it starts on,
	 * and then turn the mark off. */
	mb_lineno_save = openfile->mark_begin->lineno;
	openfile->mark_set = FALSE;
    }
#endif

    endwin();

    /* Set up an argument list to pass execvp(). */
    if (spellargs == NULL) {
	spellargs = (char **)nmalloc(arglen * sizeof(char *));

	spellargs[0] = strtok(alt_speller, " ");
	while ((ptr = strtok(NULL, " ")) != NULL) {
	    arglen++;
	    spellargs = (char **)nrealloc(spellargs, arglen *
		sizeof(char *));
	    spellargs[arglen - 3] = ptr;
	}
	spellargs[arglen - 1] = NULL;
    }
    spellargs[arglen - 2] = tempfile_name;

    /* Start a new process for the alternate speller. */
    if ((pid_spell = fork()) == 0) {
	/* Start alternate spell program; we are using PATH. */
	execvp(spellargs[0], spellargs);

	/* Should not be reached, if alternate speller is found!!! */
	exit(1);
    }

    /* If we couldn't fork, get out. */
    if (pid_spell < 0)
	return _("Could not fork");

#ifndef NANO_SMALL
    /* Don't handle a pending SIGWINCH until the alternate spell checker
     * is finished and we've loaded the spell-checked file back in. */
    allow_pending_sigwinch(FALSE);
#endif

    /* Wait for the alternate spell checker to finish. */
    wait(&alt_spell_status);

    refresh();

    /* Restore the terminal to its previous state. */
    terminal_init();

    /* Turn the cursor back on for sure. */
    curs_set(1);

    /* The screen might have been resized.  If it has, reinitialize all
     * the windows based on the new screen dimensions. */
    window_init();

    if (!WIFEXITED(alt_spell_status) ||
		WEXITSTATUS(alt_spell_status) != 0) {
	char *altspell_error;
	char *invoke_error = _("Error invoking \"%s\"");

#ifndef NANO_SMALL
	/* Turn the mark back on if it was on before. */
	openfile->mark_set = old_mark_set;
#endif

	altspell_error =
		charalloc(strlen(invoke_error) +
		strlen(alt_speller) + 1);
	sprintf(altspell_error, invoke_error, alt_speller);
	return altspell_error;
    }

#ifndef NANO_SMALL
    if (old_mark_set) {
	/* If the mark was on, partition the filestruct so that it
	 * contains only the marked text, and keep track of whether the
	 * temp file (which should contain the spell-checked marked
	 * text) will have a magicline added when it's reloaded. */
	mark_order((const filestruct **)&top, &top_x,
		(const filestruct **)&bot, &bot_x, &right_side_up);
	filepart = partition_filestruct(top, top_x, bot, bot_x);
	added_magicline = (openfile->filebot->data[0] != '\0');

	/* Get the number of characters in the marked text, and subtract
	 * it from the saved value of totsize. */
	totsize_save -= get_totsize(top, bot);
    }
#endif

    /* Reinitialize the text of the current buffer. */
    free_filestruct(openfile->fileage);
    initialize_buffer_text();

    /* Reload the temp file.  Open it, read it into the current buffer,
     * and move back to the first line of the buffer. */
    open_file(tempfile_name, FALSE, &f);
    read_file(f, tempfile_name);
    openfile->current = openfile->fileage;

#ifndef NANO_SMALL
    if (old_mark_set) {
	filestruct *top_save = openfile->fileage;

	/* If the mark was on and we added a magicline, remove it
	 * now. */
	if (added_magicline)
	    remove_magicline();

	/* Put the beginning and the end of the mark at the beginning
	 * and the end of the spell-checked text. */
	if (openfile->fileage == openfile->filebot)
	    bot_x += top_x;
	if (right_side_up) {
	    openfile->mark_begin_x = top_x;
	    current_x_save = bot_x;
	} else {
	    current_x_save = top_x;
	    openfile->mark_begin_x = bot_x;
	}

	/* Unpartition the filestruct so that it contains all the text
	 * again.  Note that we've replaced the marked text originally
	 * in the partition with the spell-checked marked text in the
	 * temp file. */
	unpartition_filestruct(&filepart);

	/* Renumber starting with the beginning line of the old
	 * partition.  Also add the number of characters in the
	 * spell-checked marked text to the saved value of totsize, and
	 * then make that saved value the actual value. */
	renumber(top_save);
	totsize_save += openfile->totsize;
	openfile->totsize = totsize_save;

	/* Assign mark_begin to the line where the mark began before. */
	do_gotopos(mb_lineno_save, openfile->mark_begin_x,
		current_y_save, 0);
	openfile->mark_begin = openfile->current;

	/* Assign mark_begin_x to the location in mark_begin where the
	 * mark began before, adjusted for any shortening of the
	 * line. */
	openfile->mark_begin_x = openfile->current_x;

	/* Turn the mark back on. */
	openfile->mark_set = TRUE;
    }
#endif

    /* Go back to the old position, and mark the file as modified. */
    do_gotopos(lineno_save, current_x_save, current_y_save, pww_save);
    set_modified();

#ifndef NANO_SMALL
    /* Handle a pending SIGWINCH again. */
    allow_pending_sigwinch(TRUE);
#endif

    return NULL;
}

void do_spell(void)
{
    int i;
    FILE *temp_file;
    char *temp = safe_tempfile(&temp_file);
    const char *spell_msg;

    if (temp == NULL) {
	statusbar(_("Could not create temp file: %s"), strerror(errno));
	return;
    }

#ifndef NANO_SMALL
    if (openfile->mark_set)
	i = write_marked_file(temp, temp_file, TRUE, FALSE);
    else
#endif
	i = write_file(temp, temp_file, TRUE, FALSE, FALSE);

    if (i == -1) {
	statusbar(_("Error writing temp file: %s"), strerror(errno));
	free(temp);
	return;
    }

    spell_msg = (alt_speller != NULL) ? do_alt_speller(temp) :
	do_int_speller(temp);
    unlink(temp);
    free(temp);

    /* If the spell-checker printed any error messages onscreen, make
     * sure that they're cleared off. */
    total_refresh();

    if (spell_msg != NULL) {
	if (errno == 0)
	    /* Don't display an error message of "Success". */
	    statusbar(_("Spell checking failed: %s"), spell_msg);
	else
	    statusbar(_("Spell checking failed: %s: %s"), spell_msg,
		strerror(errno));
    } else
	statusbar(_("Finished checking spelling"));
}
#endif /* !DISABLE_SPELLER */

#ifndef NANO_SMALL
void do_wordlinechar_count(void)
{
    size_t words = 0, chars = 0;
    ssize_t lines = 0;
    size_t current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    filestruct *current_save = openfile->current;
    bool old_mark_set = openfile->mark_set;
    bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
    filestruct *top, *bot;
    size_t top_x, bot_x;

    if (old_mark_set) {
	/* If the mark is on, partition the filestruct so that it
	 * contains only the marked text, keep track of whether the text
	 * will need a magicline added while we're counting words, add
	 * the magicline if necessary, and turn the mark off. */
	mark_order((const filestruct **)&top, &top_x,
	    (const filestruct **)&bot, &bot_x, NULL);
	filepart = partition_filestruct(top, top_x, bot, bot_x);
	if ((added_magicline = (openfile->filebot->data[0] != '\0')))
	    new_magicline();
	openfile->mark_set = FALSE;
    }

    /* Start at the top of the file. */
    openfile->current = openfile->fileage;
    openfile->current_x = 0;
    openfile->placewewant = 0;

    /* Keep moving to the next word (counting punctuation characters as
     * part of a word, as "wc -w" does), without updating the screen,
     * until we reach the end of the file, incrementing the total word
     * count whenever we're on a word just before moving. */
    while (openfile->current != openfile->filebot ||
	openfile->current_x != 0) {
	if (do_next_word(TRUE, FALSE))
	    words++;
    }

    /* Get the total line and character counts, as "wc -l"  and "wc -c"
     * do, but get the latter in multibyte characters. */
    if (old_mark_set) {
	/* If the mark was on and we added a magicline, remove it
	 * now. */
	if (added_magicline)
	    remove_magicline();

	lines = openfile->filebot->lineno -
		openfile->fileage->lineno + 1;
	chars = get_totsize(openfile->fileage, openfile->filebot);

	/* Unpartition the filestruct so that it contains all the text
	 * again, and turn the mark back on. */
	unpartition_filestruct(&filepart);
	openfile->mark_set = TRUE;
    } else {
	lines = openfile->filebot->lineno;
	chars = openfile->totsize;
    }

    /* Restore where we were. */
    openfile->current = current_save;
    openfile->current_x = current_x_save;
    openfile->placewewant = pww_save;

    /* Display the total word, line, and character counts on the
     * statusbar. */
    statusbar("%sWords: %lu  Lines: %ld  Chars: %lu", old_mark_set ?
	_("(In Selection)  ") : "", (unsigned long)words, (long)lines,
	(unsigned long)chars);
}
#endif /* !NANO_SMALL */