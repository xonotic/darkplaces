#ifndef BUTTERCHAT_H
#define BUTTERCHAT_H

#include <string.h>

#include "quakedef.h"
#include "cvar.h"


#define MAGIC_SENDER_END_DELIM "^7: " /* DO NOT TOUCH OR CHANGE */

static int BC_SelfReferred(const char *line);

#endif /* BUTTERCHAT_H */

#if defined(BC_IMPLEMENTATION) && !defined(BUTTERCHAT_IMPL_DONE)
#define BUTTERCHAT_IMPL_DONE

static const char *BC_GetSelfName(void)
{
	return Cvar_VariableString("_cl_name");
}

/* word characters: [A-Za-z0-9_] plus any byte >= 0x80 (UTF-8 letters) */
static inline int BC_IsWordChar(unsigned char c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z') || c == '_' || c >= 0x80;
}

/* Looks for a word with a fixed boundary, sort of like regex "\b<word>\b" */
static const char *BC_FindWord(const char *text, const char *word) {
	size_t n = strlen(word);
	for (const char *p = text; (p = strstr(p, word)); p++) {
		// Check if the char before the word is not a character.
		int before_ok = (p == text) || !BC_IsWordChar((unsigned char)p[-1]);
		// Check if char after the match is not a character.
		int after_ok = !BC_IsWordChar((unsigned char)p[n]);
		if (before_ok && after_ok)
			return p;
	}
	return NULL;
}

static int BC_SelfReferred(const char *line)
{
	const char *self;
	char own[MAX_SCOREBOARDNAME + 8]; /* name + MAGIC_SENDER_END_DELIM + NULL */

	self = BC_GetSelfName();
	if (!line || !self || !*self) return 0;

	/* Look for the first occurrence of our nick + MAGIC_SENDER_END_DELIM.
	 *
	 * FIXME: Players can break it tho, if they add your nice + MAGIC_SENDER_END_DELIM
	 * in their nick, in that case any mention of your name in their message
	 * will be ignored.
	 */
	dpsnprintf(own, sizeof(own), "%s%s", self, MAGIC_SENDER_END_DELIM);
	if (strstr(line, own))
		return 0; /* It's us don't highlight us */

	return BC_FindWord(line, self) != NULL;
}

#endif /* BC_IMPLEMENTATION */

