/* $Id$ */
/**************************************************************************
 *   prompt.c                                                             *
 *                                                                        *
 *   Copyright (C) 1999-2005 Chris Allegretta                             *
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "proto.h"

static char *prompt = NULL;
				/* The prompt string for statusbar
				 * questions. */
static size_t statusbar_x = (size_t)-1;
				/* The cursor position in answer. */
static size_t statusbar_pww = (size_t)-1;
				/* The place we want in answer. */
static bool reset_statusbar_x = FALSE;
				/* Should we reset the cursor position
				 * at the statusbar prompt? */

int do_statusbar_input(bool *meta_key, bool *func_key, bool *s_or_t,
	bool *ran_func, bool *finished, bool allow_funcs)
{
    int input;
	/* The character we read in. */
    static int *kbinput = NULL;
	/* The input buffer. */
    static size_t kbinput_len = 0;
	/* The length of the input buffer. */
    const shortcut *s;
    bool have_shortcut;

    *s_or_t = FALSE;
    *ran_func = FALSE;
    *finished = FALSE;

    /* Read in a character. */
    input = get_kbinput(bottomwin, meta_key, func_key);

#ifndef DISABLE_MOUSE
    /* If we got a mouse click and it was on a shortcut, read in the
     * shortcut character. */
    if (allow_funcs && *func_key == TRUE && input == KEY_MOUSE) {
	if (do_statusbar_mouse())
	    input = get_kbinput(bottomwin, meta_key, func_key);
	else
	    input = ERR;
    }
#endif

    /* Check for a shortcut in the current list. */
    s = get_shortcut(currshortcut, &input, meta_key, func_key);

    /* If we got a shortcut from the current list, or a "universal"
     * statusbar prompt shortcut, set have_shortcut to TRUE. */
    have_shortcut = (s != NULL || input == NANO_REFRESH_KEY ||
	input == NANO_HOME_KEY || input == NANO_END_KEY ||
	input == NANO_FORWARD_KEY || input == NANO_BACK_KEY ||
	input == NANO_BACKSPACE_KEY || input == NANO_DELETE_KEY ||
	input == NANO_CUT_KEY ||
#ifndef NANO_TINY
		input == NANO_NEXTWORD_KEY ||
#endif
		(*meta_key == TRUE && (
#ifndef NANO_TINY
		input == NANO_PREVWORD_KEY ||
#endif
		input == NANO_VERBATIM_KEY
#ifndef NANO_TINY
		|| input == NANO_BRACKET_KEY
#endif
		)));

    /* Set s_or_t to TRUE if we got a shortcut. */
    *s_or_t = have_shortcut;

    if (allow_funcs) {
	/* If we got a character, and it isn't a shortcut or toggle,
	 * it's a normal text character.  Display the warning if we're
	 * in view mode, or add the character to the input buffer if
	 * we're not. */
	if (input != ERR && *s_or_t == FALSE) {
	    /* If we're using restricted mode, the filename isn't blank,
	     * and we're at the "Write File" prompt, disable text
	     * input. */
	    if (!ISSET(RESTRICTED) || openfile->filename[0] == '\0' ||
		currshortcut != writefile_list) {
		kbinput_len++;
		kbinput = (int *)nrealloc(kbinput, kbinput_len *
			sizeof(int));
		kbinput[kbinput_len - 1] = input;
	    }
	}

	/* If we got a shortcut, or if there aren't any other characters
	 * waiting after the one we read in, we need to display all the
	 * characters in the input buffer if it isn't empty. */
	 if (*s_or_t == TRUE || get_key_buffer_len() == 0) {
	    if (kbinput != NULL) {
		/* Display all the characters in the input buffer at
		 * once, filtering out control characters. */
		char *output = charalloc(kbinput_len + 1);
		size_t i;
		bool got_enter;
			/* Whether we got the Enter key. */

		for (i = 0; i < kbinput_len; i++)
		    output[i] = (char)kbinput[i];
		output[i] = '\0';

		do_statusbar_output(output, kbinput_len, &got_enter,
			FALSE);

		free(output);

		/* Empty the input buffer. */
		kbinput_len = 0;
		free(kbinput);
		kbinput = NULL;
	    }
	}

	if (have_shortcut) {
	    switch (input) {
		/* Handle the "universal" statusbar prompt shortcuts. */
		case NANO_REFRESH_KEY:
		    total_refresh();
		    break;
		case NANO_HOME_KEY:
		    do_statusbar_home();
		    break;
		case NANO_END_KEY:
		    do_statusbar_end();
		    break;
		case NANO_FORWARD_KEY:
		    do_statusbar_right();
		    break;
		case NANO_BACK_KEY:
		    do_statusbar_left();
		    break;
		case NANO_BACKSPACE_KEY:
		    /* If we're using restricted mode, the filename
		     * isn't blank, and we're at the "Write File"
		     * prompt, disable Backspace. */
		    if (!ISSET(RESTRICTED) || openfile->filename[0] ==
			'\0' || currshortcut != writefile_list)
			do_statusbar_backspace();
		    break;
		case NANO_DELETE_KEY:
		    /* If we're using restricted mode, the filename
		     * isn't blank, and we're at the "Write File"
		     * prompt, disable Delete. */
		    if (!ISSET(RESTRICTED) || openfile->filename[0] ==
			'\0' || currshortcut != writefile_list)
			do_statusbar_delete();
		    break;
		case NANO_CUT_KEY:
		    /* If we're using restricted mode, the filename
		     * isn't blank, and we're at the "Write File"
		     * prompt, disable Cut. */
		    if (!ISSET(RESTRICTED) || openfile->filename[0] ==
			'\0' || currshortcut != writefile_list)
			do_statusbar_cut_text();
		    break;
#ifndef NANO_TINY
		case NANO_NEXTWORD_KEY:
		    do_statusbar_next_word(FALSE);
		    break;
		case NANO_PREVWORD_KEY:
		    if (*meta_key == TRUE)
			do_statusbar_prev_word(FALSE);
		    break;
#endif
		case NANO_VERBATIM_KEY:
		    if (*meta_key == TRUE) {
			/* If we're using restricted mode, the filename
			 * isn't blank, and we're at the "Write File"
			 * prompt, disable verbatim input. */
			if (!ISSET(RESTRICTED) ||
				openfile->filename[0] == '\0' ||
				currshortcut != writefile_list) {
			    bool got_enter;
				/* Whether we got the Enter key. */

			    do_statusbar_verbatim_input(&got_enter);

			    /* If we got the Enter key, set input to the
			     * key value for Enter, and set finished to
			     * TRUE to indicate that we're done. */
			    if (got_enter) {
				input = NANO_ENTER_KEY;
				*finished = TRUE;
			    }
			}
		    }
		    break;
#ifndef NANO_TINY
		case NANO_BRACKET_KEY:
		    if (*meta_key == TRUE)
			do_statusbar_find_bracket();
		    break;
#endif
		/* Handle the normal statusbar prompt shortcuts, setting
		 * ran_func to TRUE if we try to run their associated
		 * functions and setting finished to TRUE to indicate
		 * that we're done after trying to run their associated
		 * functions. */
		default:
		    if (s->func != NULL) {
			*ran_func = TRUE;
			if (!ISSET(VIEW_MODE) || s->viewok)
			    s->func();
		    }
		    *finished = TRUE;
	    }
	}
    }

    return input;
}

#ifndef DISABLE_MOUSE
bool do_statusbar_mouse(void)
{
    int mouse_x, mouse_y;
    bool retval = get_mouseinput(&mouse_x, &mouse_y, TRUE);

    if (!retval) {
	/* We can click in the statusbar window text to move the
	 * cursor. */
	if (wenclose(bottomwin, mouse_y, mouse_x)) {
	    size_t start_col;

	    assert(prompt != NULL);

	    start_col = strlenpt(prompt) + 1;

	    /* Subtract out the sizes of topwin and edit. */
	    mouse_y -= (2 - no_more_space()) + editwinrows;

	    /* Move to where the click occurred. */
	    if (mouse_x > start_col && mouse_y == 0) {
		size_t pww_save = statusbar_pww;

		statusbar_x = actual_x(answer,
			get_statusbar_page_start(start_col, start_col +
			statusbar_xplustabs()) + mouse_x - start_col -
			1);
		statusbar_pww = statusbar_xplustabs();

		if (need_statusbar_horizontal_update(pww_save))
		    update_statusbar_line(answer, statusbar_x);
	    }
	}
    }

    return retval;
}
#endif

/* The user typed output_len multibyte characters.  Add them to the
 * statusbar prompt, setting got_enter to TRUE if we get a newline, and
 * filtering out all control characters if allow_cntrls is TRUE. */
void do_statusbar_output(char *output, size_t output_len, bool
	*got_enter, bool allow_cntrls)
{
    size_t answer_len, i = 0;
    char *char_buf = charalloc(mb_cur_max());
    int char_buf_len;

    assert(answer != NULL);

    answer_len = strlen(answer);
    *got_enter = FALSE;

    while (i < output_len) {
	/* If allow_cntrls is FALSE, filter out nulls and newlines,
	 * since they're control characters. */
	if (allow_cntrls) {
	    /* Null to newline, if needed. */
	    if (output[i] == '\0')
		output[i] = '\n';
	    /* Newline to Enter, if needed. */
	    else if (output[i] == '\n') {
		/* Set got_enter to TRUE to indicate that we got the
		 * Enter key, put back the rest of the characters in
		 * output so that they can be parsed and output again,
		 * and get out. */
		*got_enter = TRUE;
		unparse_kbinput(output + i, output_len - i);
		return;
	    }
	}

	/* Interpret the next multibyte character. */
	char_buf_len = parse_mbchar(output + i, char_buf, NULL);

	i += char_buf_len;

	/* If allow_cntrls is FALSE, filter out a control character. */
	if (!allow_cntrls && is_cntrl_mbchar(output + i - char_buf_len))
	    continue;

	/* More dangerousness fun =) */
	answer = charealloc(answer, answer_len + (char_buf_len * 2));

	assert(statusbar_x <= answer_len);

	charmove(answer + statusbar_x + char_buf_len,
		answer + statusbar_x, answer_len - statusbar_x +
		char_buf_len);
	strncpy(answer + statusbar_x, char_buf, char_buf_len);
	answer_len += char_buf_len;

	statusbar_x += char_buf_len;
    }

    free(char_buf);

    statusbar_pww = statusbar_xplustabs();

    update_statusbar_line(answer, statusbar_x);
}

void do_statusbar_home(void)
{
    size_t pww_save = statusbar_pww;

#ifndef NANO_TINY
    if (ISSET(SMART_HOME)) {
	size_t statusbar_x_save = statusbar_x;

	statusbar_x = indent_length(answer);

	if (statusbar_x == statusbar_x_save ||
		statusbar_x == strlen(answer))
	    statusbar_x = 0;

	statusbar_pww = statusbar_xplustabs();
    } else {
#endif
	statusbar_x = 0;
	statusbar_pww = statusbar_xplustabs();
#ifndef NANO_TINY
    }
#endif

    if (need_statusbar_horizontal_update(pww_save))
	update_statusbar_line(answer, statusbar_x);
}

void do_statusbar_end(void)
{
    size_t pww_save = statusbar_pww;

    statusbar_x = strlen(answer);
    statusbar_pww = statusbar_xplustabs();

    if (need_statusbar_horizontal_update(pww_save))
	update_statusbar_line(answer, statusbar_x);
}

void do_statusbar_right(void)
{
    if (statusbar_x < strlen(answer)) {
	size_t pww_save = statusbar_pww;

	statusbar_x = move_mbright(answer, statusbar_x);
	statusbar_pww = statusbar_xplustabs();

	if (need_statusbar_horizontal_update(pww_save))
	    update_statusbar_line(answer, statusbar_x);
    }
}

void do_statusbar_left(void)
{
    if (statusbar_x > 0) {
	size_t pww_save = statusbar_pww;

	statusbar_x = move_mbleft(answer, statusbar_x);
	statusbar_pww = statusbar_xplustabs();

	if (need_statusbar_horizontal_update(pww_save))
	    update_statusbar_line(answer, statusbar_x);
    }
}

void do_statusbar_backspace(void)
{
    if (statusbar_x > 0) {
	do_statusbar_left();
	do_statusbar_delete();
    }
}

void do_statusbar_delete(void)
{
    statusbar_pww = statusbar_xplustabs();

    if (answer[statusbar_x] != '\0') {
	int char_buf_len = parse_mbchar(answer + statusbar_x, NULL,
		NULL);
	size_t line_len = strlen(answer + statusbar_x);

	assert(statusbar_x < strlen(answer));

	charmove(answer + statusbar_x, answer + statusbar_x +
		char_buf_len, strlen(answer) - statusbar_x -
		char_buf_len + 1);

	null_at(&answer, statusbar_x + line_len - char_buf_len);

	update_statusbar_line(answer, statusbar_x);
    }
}

/* Move text from the statusbar prompt into oblivion. */
void do_statusbar_cut_text(void)
{
    assert(answer != NULL);

#ifndef NANO_TINY
    if (ISSET(CUT_TO_END))
	null_at(&answer, statusbar_x);
    else {
#endif
	null_at(&answer, 0);
	statusbar_x = 0;
	statusbar_pww = statusbar_xplustabs();
#ifndef NANO_TINY
    }
#endif

    update_statusbar_line(answer, statusbar_x);
}

#ifndef NANO_TINY
/* Move to the next word at the statusbar prompt.  If allow_punct is
 * TRUE, treat punctuation as part of a word.  Return TRUE if we started
 * on a word, and FALSE otherwise. */
bool do_statusbar_next_word(bool allow_punct)
{
    size_t pww_save = statusbar_pww;
    char *char_mb;
    int char_mb_len;
    bool end_line = FALSE, started_on_word = FALSE;

    assert(answer != NULL);

    char_mb = charalloc(mb_cur_max());

    /* Move forward until we find the character after the last letter of
     * the current word. */
    while (!end_line) {
	char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

	/* If we've found it, stop moving forward through the current
	 * line. */
	if (!is_word_mbchar(char_mb, allow_punct))
	    break;

	/* If we haven't found it, then we've started on a word, so set
	 * started_on_word to TRUE. */
	started_on_word = TRUE;

	if (answer[statusbar_x] == '\0')
	    end_line = TRUE;
	else
	    statusbar_x += char_mb_len;
    }

    /* Move forward until we find the first letter of the next word. */
    if (answer[statusbar_x] == '\0')
	end_line = TRUE;
    else
	statusbar_x += char_mb_len;

    while (!end_line) {
	char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

	/* If we've found it, stop moving forward through the current
	 * line. */
	if (is_word_mbchar(char_mb, allow_punct))
	    break;

	if (answer[statusbar_x] == '\0')
	    end_line = TRUE;
	else
	    statusbar_x += char_mb_len;
    }

    free(char_mb);

    statusbar_pww = statusbar_xplustabs();

    if (need_statusbar_horizontal_update(pww_save))
	update_statusbar_line(answer, statusbar_x);

    /* Return whether we started on a word. */
    return started_on_word;
}

/* Move to the previous word at the statusbar prompt.  If allow_punct is
 * TRUE, treat punctuation as part of a word.  Return TRUE if we started
 * on a word, and FALSE otherwise. */
bool do_statusbar_prev_word(bool allow_punct)
{
    size_t pww_save = statusbar_pww;
    char *char_mb;
    int char_mb_len;
    bool begin_line = FALSE, started_on_word = FALSE;

    assert(answer != NULL);

    char_mb = charalloc(mb_cur_max());

    /* Move backward until we find the character before the first letter
     * of the current word. */
    while (!begin_line) {
	char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

	/* If we've found it, stop moving backward through the current
	 * line. */
	if (!is_word_mbchar(char_mb, allow_punct))
	    break;

	/* If we haven't found it, then we've started on a word, so set
	 * started_on_word to TRUE. */
	started_on_word = TRUE;

	if (statusbar_x == 0)
	    begin_line = TRUE;
	else
	    statusbar_x = move_mbleft(answer, statusbar_x);
    }

    /* Move backward until we find the last letter of the previous
     * word. */
    if (statusbar_x == 0)
	begin_line = TRUE;
    else
	statusbar_x = move_mbleft(answer, statusbar_x);

    while (!begin_line) {
	char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

	/* If we've found it, stop moving backward through the current
	 * line. */
	if (is_word_mbchar(char_mb, allow_punct))
	    break;

	if (statusbar_x == 0)
	    begin_line = TRUE;
	else
	    statusbar_x = move_mbleft(answer, statusbar_x);
    }

    /* If we've found it, move backward until we find the character
     * before the first letter of the previous word. */
    if (!begin_line) {
	if (statusbar_x == 0)
	    begin_line = TRUE;
	else
	    statusbar_x = move_mbleft(answer, statusbar_x);

	while (!begin_line) {
	    char_mb_len = parse_mbchar(answer + statusbar_x, char_mb,
		NULL);

	    /* If we've found it, stop moving backward through the
	     * current line. */
	    if (!is_word_mbchar(char_mb, allow_punct))
		break;

	    if (statusbar_x == 0)
		begin_line = TRUE;
	    else
		statusbar_x = move_mbleft(answer, statusbar_x);
	}

	/* If we've found it, move forward to the first letter of the
	 * previous word. */
	if (!begin_line)
	    statusbar_x += char_mb_len;
    }

    free(char_mb);

    statusbar_pww = statusbar_xplustabs();

    if (need_statusbar_horizontal_update(pww_save))
	update_statusbar_line(answer, statusbar_x);

    /* Return whether we started on a word. */
    return started_on_word;
}
#endif /* !NANO_TINY */

void do_statusbar_verbatim_input(bool *got_enter)
{
    int *kbinput;
    size_t kbinput_len, i;
    char *output;

    *got_enter = FALSE;

    /* Read in all the verbatim characters. */
    kbinput = get_verbatim_kbinput(bottomwin, &kbinput_len);

    /* Display all the verbatim characters at once, not filtering out
     * control characters. */
    output = charalloc(kbinput_len + 1);

    for (i = 0; i < kbinput_len; i++)
	output[i] = (char)kbinput[i];
    output[i] = '\0';

    do_statusbar_output(output, kbinput_len, got_enter, TRUE);

    free(output);
}

#ifndef NANO_TINY
bool find_statusbar_bracket_match(bool reverse, const char
	*bracket_set)
{
    const char *rev_start = NULL, *found = NULL;

    assert(strlen(bracket_set) == 2);

    /* rev_start might end up 1 character before the start or after the
     * end of the line.  This won't be a problem because we'll skip over
     * it below in that case. */
    rev_start = reverse ? answer + (statusbar_x - 1) : answer +
	(statusbar_x + 1);

    while (TRUE) {
	/* Look for either of the two characters in bracket_set.
	 * rev_start can be 1 character before the start or after the
	 * end of the line.  In either case, just act as though no match
	 * is found. */
	found = ((rev_start > answer && *(rev_start - 1) == '\0') ||
		rev_start < answer) ? NULL : (reverse ?
		revstrpbrk(answer, bracket_set, rev_start) :
		strpbrk(rev_start, bracket_set));

	/* We've found a potential match. */
	if (found != NULL)
	    break;

	/* We've reached the start or end of the statusbar text, so
	 * get out. */
	return FALSE;
    }

    /* We've definitely found something. */
    statusbar_x = found - answer;
    statusbar_pww = statusbar_xplustabs();

    return TRUE;
}

void do_statusbar_find_bracket(void)
{
    size_t statusbar_x_save, pww_save;
    const char *bracket_list = "()<>[]{}";
	/* The list of brackets we can find matches to. */
    const char *pos;
	/* The location in bracket_list of the bracket at the current
	 * cursor position. */
    char ch;
	/* The bracket at the current cursor position. */
    char wanted_ch;
	/* The bracket complementing the bracket at the current cursor
	 * position. */
    char bracket_set[3];
	/* The pair of characters in ch and wanted_ch. */
    size_t count = 1;
	/* The initial bracket count. */
    bool reverse;
	/* The direction we search. */

    assert(strlen(bracket_list) % 2 == 0);

    ch = answer[statusbar_x];

    if (ch == '\0' || (pos = strchr(bracket_list, ch)) == NULL)
	return;

    /* Save where we are. */
    statusbar_x_save = statusbar_x;
    pww_save = statusbar_pww;

    /* If we're on an opening bracket, we want to search forwards for a
     * closing bracket, and if we're on a closing bracket, we want to
     * search backwards for an opening bracket. */
    reverse = ((pos - bracket_list) % 2 != 0);
    wanted_ch = reverse ? *(pos - 1) : *(pos + 1);
    bracket_set[0] = ch;
    bracket_set[1] = wanted_ch;
    bracket_set[2] = '\0';

    while (TRUE) {
	if (find_statusbar_bracket_match(reverse, bracket_set)) {
	    /* If we found an identical bracket, increment count.  If we
	     * found a complementary bracket, decrement it. */
	    count += (answer[statusbar_x] == ch) ? 1 : -1;

	    /* If count is zero, we've found a matching bracket.  Update
	     * the statusbar prompt and get out. */
	    if (count == 0) {
		if (need_statusbar_horizontal_update(pww_save))
		    update_statusbar_line(answer, statusbar_x);
		break;
	    }
	} else {
	    /* We didn't find either an opening or closing bracket.
	     * Restore where we were, and get out. */
	    statusbar_x = statusbar_x_save;
	    statusbar_pww = pww_save;
	    break;
	}
    }
}
#endif /* !NANO_TINY */

/* Return the placewewant associated with statusbar_x, i.e, the
 * zero-based column position of the cursor.  The value will be no
 * smaller than statusbar_x. */
size_t statusbar_xplustabs(void)
{
    return strnlenpt(answer, statusbar_x);
}

/* nano scrolls horizontally within a line in chunks.  This function
 * returns the column number of the first character displayed in the
 * statusbar prompt when the cursor is at the given column with the
 * prompt ending at start_col.  Note that (0 <= column -
 * get_statusbar_page_start(column) < COLS). */
size_t get_statusbar_page_start(size_t start_col, size_t column)
{
    if (column == start_col || column < COLS - 1)
	return 0;
    else
	return column - start_col - (column - start_col) % (COLS -
		start_col - 1);
}

/* Repaint the statusbar when getting a character in
 * get_prompt_string().  The statusbar text line will be displayed
 * starting with curranswer[index].  Assume the A_REVERSE attribute is
 * turned off. */
void update_statusbar_line(const char *curranswer, size_t index)
{
    size_t start_col, page_start;
    char *expanded;

    assert(prompt != NULL && index <= strlen(buf));

    start_col = strlenpt(prompt) + 1;
    index = strnlenpt(curranswer, index);
    page_start = get_statusbar_page_start(start_col, start_col + index);

    wattron(bottomwin, A_REVERSE);

    blank_statusbar();

    mvwaddnstr(bottomwin, 0, 0, prompt, actual_x(prompt, COLS - 2));
    waddch(bottomwin, ':');
    waddch(bottomwin, (page_start == 0) ? ' ' : '$');

    expanded = display_string(curranswer, page_start, COLS - start_col -
	1, FALSE);
    waddstr(bottomwin, expanded);
    free(expanded);

    reset_statusbar_cursor();

    wattroff(bottomwin, A_REVERSE);
}

/* Put the cursor in the statusbar prompt at statusbar_x. */
void reset_statusbar_cursor(void)
{
    size_t start_col = strlenpt(prompt) + 1;
    size_t xpt = statusbar_xplustabs();

    wmove(bottomwin, 0, start_col + 1 + xpt -
	get_statusbar_page_start(start_col, start_col + xpt));
}

/* Return TRUE if we need an update after moving horizontally, and FALSE
 * otherwise.  We need one if old_pww and statusbar_pww are on different
 * pages. */
bool need_statusbar_horizontal_update(size_t old_pww)
{
    size_t start_col = strlenpt(prompt) + 1;

    return get_statusbar_page_start(start_col, start_col + old_pww) !=
	get_statusbar_page_start(start_col, start_col + statusbar_pww);
}

/* Get a string of input at the statusbar prompt.  This should only be
 * called from do_prompt(). */
int get_prompt_string(bool allow_tabs, const char *curranswer,
#ifndef NANO_TINY
	filestruct **history_list,
#endif
	const shortcut *s
#ifndef DISABLE_TABCOMP
	, bool *list
#endif
	)
{
    int kbinput;
    bool meta_key, func_key, s_or_t, ran_func, finished;
    size_t curranswer_len;
#ifndef DISABLE_TABCOMP
    bool tabbed = FALSE;
	/* Whether we've pressed Tab. */
#endif
#ifndef NANO_TINY
    char *history = NULL;
	/* The current history string. */
    char *magichistory = NULL;
	/* The temporary string typed at the bottom of the history, if
	 * any. */
#ifndef DISABLE_TABCOMP
    int last_kbinput = ERR;
	/* The key we pressed before the current key. */
    size_t complete_len = 0;
	/* The length of the original string that we're trying to
	 * tab complete, if any. */
#endif
#endif /* !NANO_TINY */

    answer = mallocstrcpy(answer, curranswer);
    curranswer_len = strlen(answer);

    /* Only put statusbar_x at the end of the string (and change
     * statusbar_pww to match) if it's uninitialized, if it would be
     * past the end of curranswer, or if reset_statusbar_x is TRUE.
     * Otherwise, leave it alone.  This is so the cursor position stays
     * at the same place if a prompt-changing toggle is pressed. */
    if (statusbar_x == (size_t)-1 || statusbar_x > curranswer_len ||
		reset_statusbar_x) {
	statusbar_x = curranswer_len;
	statusbar_pww = statusbar_xplustabs();
    }

    currshortcut = s;

    update_statusbar_line(answer, statusbar_x);

    /* Refresh the edit window and the statusbar before getting
     * input. */
    wnoutrefresh(edit);
    wnoutrefresh(bottomwin);

    /* If we're using restricted mode, we aren't allowed to change the
     * name of a file once it has one because that would allow writing
     * to files not specified on the command line.  In this case,
     * disable all keys that would change the text if the filename isn't
     * blank and we're at the "Write File" prompt. */
    while ((kbinput = do_statusbar_input(&meta_key, &func_key,
	&s_or_t, &ran_func, &finished, TRUE)) != NANO_CANCEL_KEY &&
	kbinput != NANO_ENTER_KEY) {

	assert(statusbar_x <= strlen(answer));

#ifndef DISABLE_TABCOMP
	if (kbinput != NANO_TAB_KEY)
	    tabbed = FALSE;
#endif

	switch (kbinput) {
#ifndef DISABLE_TABCOMP
#ifndef NANO_TINY
	    case NANO_TAB_KEY:
		if (history_list != NULL) {
		    if (last_kbinput != NANO_TAB_KEY)
			complete_len = strlen(answer);

		    if (complete_len > 0) {
			answer = mallocstrcpy(answer,
				get_history_completion(history_list,
				answer, complete_len));
			statusbar_x = strlen(answer);
		    }
		} else
#endif /* !NANO_TINY */
		if (allow_tabs)
		    answer = input_tab(answer, &statusbar_x, &tabbed,
			list);

		update_statusbar_line(answer, statusbar_x);
		break;
#endif /* !DISABLE_TABCOMP */
#ifndef NANO_TINY
	    case NANO_PREVLINE_KEY:
		if (history_list != NULL) {
		    /* If we're scrolling up at the bottom of the
		     * history list and answer isn't blank, save answer
		     * in magichistory. */
		    if ((*history_list)->next == NULL &&
			answer[0] != '\0')
			magichistory = mallocstrcpy(magichistory,
				answer);

		    /* Get the older search from the history list and
		     * save it in answer.  If there is no older search,
		     * don't do anything. */
		    if ((history =
			get_history_older(history_list)) != NULL) {
			answer = mallocstrcpy(answer, history);
			statusbar_x = strlen(answer);
		    }

		    update_statusbar_line(answer, statusbar_x);

		    /* This key has a shortcut list entry when it's used
		     * to move to an older search, which means that
		     * finished has been set to TRUE.  Set it back to
		     * FALSE here, so that we aren't kicked out of the
		     * statusbar prompt. */
		    finished = FALSE;
		}
		break;
	    case NANO_NEXTLINE_KEY:
		if (history_list != NULL) {
		    /* Get the newer search from the history list and
		     * save it in answer.  If there is no newer search,
		     * don't do anything. */
		    if ((history =
			get_history_newer(history_list)) != NULL) {
			answer = mallocstrcpy(answer, history);
			statusbar_x = strlen(answer);
		    }

		    /* If, after scrolling down, we're at the bottom of
		     * the history list, answer is blank, and
		     * magichistory is set, save magichistory in
		     * answer. */
		    if ((*history_list)->next == NULL &&
			answer[0] == '\0' && magichistory != NULL) {
			answer = mallocstrcpy(answer, magichistory);
			statusbar_x = strlen(answer);
		    }

		    update_statusbar_line(answer, statusbar_x);
		}
		break;
#endif /* !NANO_TINY */
	}

	/* If we have a shortcut with an associated function, break out
	 * if we're finished after running or trying to run the
	 * function. */
	if (finished)
	    break;

#if !defined(NANO_TINY) && !defined(DISABLE_TABCOMP)
	last_kbinput = kbinput;
#endif

	reset_statusbar_cursor();
    }

#ifndef NANO_TINY
    /* Set the current position in the history list to the bottom and
     * free magichistory, if we need to. */
    if (history_list != NULL) {
	history_reset(*history_list);

	if (magichistory != NULL)
	    free(magichistory);
    }
#endif

    /* We finished putting in an answer or ran a normal shortcut's
     * associated function, so reset statusbar_x and statusbar_pww. */
    if (kbinput == NANO_CANCEL_KEY || kbinput == NANO_ENTER_KEY ||
	ran_func) {
	statusbar_x = (size_t)-1;
	statusbar_pww = (size_t)-1;
    }

    return kbinput;
}

/* Ask a question on the statusbar.  The prompt will be stored in the
 * static prompt, which should be NULL initially, and the answer will be
 * stored in the answer global.  Returns -1 on aborted enter, -2 on a
 * blank string, and 0 otherwise, the valid shortcut key caught.
 * curranswer is any editable text that we want to put up by default.
 *
 * The allow_tabs parameter indicates whether we should allow tabs to be
 * interpreted. */
int do_prompt(bool allow_tabs, const shortcut *s, const char
	*curranswer,
#ifndef NANO_TINY
	filestruct **history_list,
#endif
	const char *msg, ...)
{
    va_list ap;
    int retval;
#ifndef DISABLE_TABCOMP
    bool list = FALSE;
#endif

    /* prompt has been freed and set to NULL unless the user resized
     * while at the statusbar prompt. */
    if (prompt != NULL)
	free(prompt);

    prompt = charalloc(((COLS - 4) * mb_cur_max()) + 1);

    bottombars(s);

    va_start(ap, msg);
    vsnprintf(prompt, (COLS - 4) * mb_cur_max(), msg, ap);
    va_end(ap);
    null_at(&prompt, actual_x(prompt, COLS - 4));

    retval = get_prompt_string(allow_tabs, curranswer,
#ifndef NANO_TINY
		history_list,
#endif
		s
#ifndef DISABLE_TABCOMP
		, &list
#endif
		);

    free(prompt);
    prompt = NULL;

    reset_statusbar_x = FALSE;

    switch (retval) {
	case NANO_CANCEL_KEY:
	    retval = -1;
	    reset_statusbar_x = TRUE;
	    break;
	case NANO_ENTER_KEY:
	    retval = (answer[0] == '\0') ? -2 : 0;
	    reset_statusbar_x = TRUE;
	    break;
    }

    blank_statusbar();
    wnoutrefresh(bottomwin);

#ifdef DEBUG
    fprintf(stderr, "answer = \"%s\"\n", answer);
#endif

#ifndef DISABLE_TABCOMP
    /* If we've done tab completion, there might be a list of filename
     * matches on the edit window at this point.  Make sure that they're
     * cleared off. */
    if (list)
	edit_refresh();
#endif

    return retval;
}

/* This function forces a reset of the statusbar cursor position.  It
 * should only be called after do_prompt(), and is only needed if the
 * user leaves the prompt via something other than Enter or Cancel. */
void do_prompt_abort(void)
{
    reset_statusbar_x = TRUE;
}

/* Ask a simple Yes/No (and optionally All) question, specified in msg,
 * on the statusbar.  Return 1 for Yes, 0 for No, 2 for All (if all is
 * TRUE when passed in), and -1 for Cancel. */
int do_yesno_prompt(bool all, const char *msg)
{
    int ok = -2, width = 16;
    const char *yesstr;		/* String of Yes characters accepted. */
    const char *nostr;		/* Same for No. */
    const char *allstr;		/* And All, surprise! */

    assert(msg != NULL);

    /* yesstr, nostr, and allstr are strings of any length.  Each string
     * consists of all single-byte characters accepted as valid
     * characters for that value.  The first value will be the one
     * displayed in the shortcuts.  Translators: if possible, specify
     * both the shortcuts for your language and English.  For example,
     * in French: "OoYy" for "Oui". */
    yesstr = _("Yy");
    nostr = _("Nn");
    allstr = _("Aa");

    if (!ISSET(NO_HELP)) {
	char shortstr[3];
		/* Temp string for Yes, No, All. */

	if (COLS < 32)
	    width = COLS / 2;

	/* Clear the shortcut list from the bottom of the screen. */
	blank_bottombars();

	sprintf(shortstr, " %c", yesstr[0]);
	wmove(bottomwin, 1, 0);
	onekey(shortstr, _("Yes"), width);

	if (all) {
	    wmove(bottomwin, 1, width);
	    shortstr[1] = allstr[0];
	    onekey(shortstr, _("All"), width);
	}

	wmove(bottomwin, 2, 0);
	shortstr[1] = nostr[0];
	onekey(shortstr, _("No"), width);

	wmove(bottomwin, 2, 16);
	onekey("^C", _("Cancel"), width);
    }

    wattron(bottomwin, A_REVERSE);

    blank_statusbar();
    mvwaddnstr(bottomwin, 0, 0, msg, actual_x(msg, COLS - 1));

    wattroff(bottomwin, A_REVERSE);

    /* Refresh the edit window and the statusbar before getting
     * input. */
    wnoutrefresh(edit);
    wnoutrefresh(bottomwin);

    do {
	int kbinput;
	bool meta_key, func_key;
#ifndef DISABLE_MOUSE
	int mouse_x, mouse_y;
#endif

	kbinput = get_kbinput(bottomwin, &meta_key, &func_key);

	if (kbinput == NANO_REFRESH_KEY) {
	    total_redraw();
	    continue;
	} else if (kbinput == NANO_CANCEL_KEY)
	    ok = -1;
#ifndef DISABLE_MOUSE
	else if (kbinput == KEY_MOUSE) {
	    get_mouseinput(&mouse_x, &mouse_y, FALSE);

	    if (mouse_x != -1 && mouse_y != -1 && !ISSET(NO_HELP) &&
		wenclose(bottomwin, mouse_y, mouse_x) &&
		mouse_x < (width * 2) && mouse_y - (2 -
		no_more_space()) - editwinrows - 1 >= 0) {
		int x = mouse_x / width;
			/* Calculate the x-coordinate relative to the
			 * two columns of the Yes/No/All shortcuts in
			 * bottomwin. */
		int y = mouse_y - (2 - no_more_space()) -
			editwinrows - 1;
			/* Calculate the y-coordinate relative to the
			 * beginning of the Yes/No/All shortcuts in
			 * bottomwin, i.e, with the sizes of topwin,
			 * edit, and the first line of bottomwin
			 * subtracted out. */

		assert(0 <= x && x <= 1 && 0 <= y && y <= 1);

		/* x == 0 means they clicked Yes or No.  y == 0 means
		 * Yes or All. */
		ok = -2 * x * y + x - y + 1;

		if (ok == 2 && !all)
		    ok = -2;
	    }
	}
#endif
	/* Look for the kbinput in the Yes, No and (optionally) All
	 * strings. */
	else if (strchr(yesstr, kbinput) != NULL)
	    ok = 1;
	else if (strchr(nostr, kbinput) != NULL)
	    ok = 0;
	else if (all && strchr(allstr, kbinput) != NULL)
	    ok = 2;
    } while (ok == -2);

    return ok;
}