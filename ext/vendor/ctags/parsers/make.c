/*
*   Copyright (c) 2000-2005, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for generating tags for makefiles.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>
#include <ctype.h>

#include "meta-make.h"

#include "kind.h"
#include "options.h"
#include "parse.h"
#include "read.h"
#include "routines.h"
#include "vstring.h"
#include "xtag.h"


/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_MACRO, K_TARGET, K_INCLUDE,
} makeKind;

typedef enum {
	R_INCLUDE_GENERIC,
	R_INCLUDE_OPTIONAL,
} makeMakefileRole;

static roleDesc MakeMakefileRoles [] = {
	{ true, "included", "included" },
	{ true, "optional", "optionally included"},
};

static kindOption MakeKinds [] = {
	{ true, 'm', "macro",  "macros"},
	{ true, 't', "target", "targets"},
	{ true, 'I', "makefile", "makefiles",
	  .referenceOnly = true, ATTACH_ROLES(MakeMakefileRoles)},
};


/*
*   FUNCTION DEFINITIONS
*/

static int nextChar (void)
{
	int c = getcFromInputFile ();
	if (c == '\\')
	{
		c = getcFromInputFile ();
		if (c == '\n')
			c = nextChar ();
	}
	return c;
}

static void skipLine (void)
{
	int c;
	do
		c = nextChar ();
	while (c != EOF  &&  c != '\n');
	if (c == '\n')
		ungetcToInputFile (c);
}

static int skipToNonWhite (int c)
{
	while (c != '\n' && isspace (c))
		c = nextChar ();
	return c;
}

static bool isIdentifier (int c)
{
	return (bool)(c != '\0' && (isalnum (c)  ||  strchr (".-_/$(){}%", c) != NULL));
}

static bool isSpecialTarget (vString *const name)
{
	size_t i = 0;
	/* All special targets begin with '.'. */
	if (vStringLength (name) < 1 || vStringChar (name, i++) != '.') {
		return false;
	}
	while (i < vStringLength (name)) {
		char ch = vStringChar (name, i++);
		if (ch != '_' && !isupper (ch))
		{
			return false;
		}
	}
	return true;
}

static void makeSimpleMakeTag (vString *const name, kindOption *MakeKinds, makeKind kind)
{
	static int make = LANG_IGNORE;

	if (make == LANG_IGNORE)
		make = getNamedLanguage ("Make", 0);

	if (! isLanguageEnabled (make))
		return;

	pushLanguage (make);
	makeSimpleTag (name, MakeKinds, kind);
	popLanguage ();
}

static void makeSimpleMakeRefTag (const vString* const name, kindOption* const kinds, const int kind,
				  int roleIndex)
{
	static int make = LANG_IGNORE;

	if (make == LANG_IGNORE)
		make = getNamedLanguage ("Make", 0);

	if (! isLanguageEnabled (make))
		return;

	pushLanguage (make);
	makeSimpleRefTag (name, kinds, kind, roleIndex);
	popLanguage ();
}

static void newTarget (vString *const name)
{
	/* Ignore GNU Make's "special targets". */
	if  (isSpecialTarget (name))
	{
		return;
	}
	makeSimpleMakeTag (name, MakeKinds, K_TARGET);
}

static void newMacro (vString *const name, bool with_define_directive, bool appending, struct makeParserClient *client, void *data)
{
	if (!appending)
		makeSimpleMakeTag (name, MakeKinds, K_MACRO);
	if (client->newMacro)
		client->newMacro (client, name, with_define_directive, appending, data);
}

static void newInclude (vString *const name, bool optional)
{
	makeSimpleMakeRefTag (name, MakeKinds, K_INCLUDE,
			      optional? R_INCLUDE_OPTIONAL: R_INCLUDE_GENERIC);
}

static bool isAcceptableAsInclude (vString *const name)
{
	if (strcmp (vStringValue (name), "$") == 0)
		return false;
	return true;
}

static void readIdentifier (const int first, vString *const id)
{
	int depth = 0;
	int c = first;
	vStringClear (id);
	while (isIdentifier (c) || (depth > 0 && c != EOF && c != '\n'))
	{
		if (c == '(' || c == '{')
			depth++;
		else if (depth > 0 && (c == ')' || c == '}'))
			depth--;
		vStringPut (id, c);
		c = nextChar ();
	}
	ungetcToInputFile (c);
}

extern void runMakeParser (struct makeParserClient *client, void *data)
{
	stringList *identifiers = stringListNew ();
	bool newline = true;
	bool in_define = false;
	bool in_value  = false;
	bool in_rule = false;
	bool variable_possible = true;
	bool appending = false;
	int c;

	static int make = LANG_IGNORE;

	if (make == LANG_IGNORE)
	{
		make = getNamedLanguage ("Make", 0);
		initializeParser (make);
	}

	while ((c = nextChar ()) != EOF)
	{
		if (newline)
		{
			if (in_rule)
			{
				if (c == '\t' || (c = skipToNonWhite (c)) == '#')
				{
					skipLine ();  /* skip rule or comment */
					c = nextChar ();
				}
				else if (c != '\n')
					in_rule = false;
			}
			else if (in_value)
				in_value = false;

			stringListClear (identifiers);
			variable_possible = (bool)(!in_rule);
			newline = false;
		}
		if (c == '\n')
			newline = true;
		else if (isspace (c))
			continue;
		else if (c == '#')
			skipLine ();
		else if (variable_possible && c == '?')
		{
			c = nextChar ();
			ungetcToInputFile (c);
			variable_possible = (c == '=');
		}
		else if (variable_possible && c == '+')
		{
			c = nextChar ();
			ungetcToInputFile (c);
			variable_possible = (c == '=');
			appending = true;
		}
		else if (variable_possible && c == ':' &&
				 stringListCount (identifiers) > 0)
		{
			c = nextChar ();
			ungetcToInputFile (c);
			if (c != '=')
			{
				unsigned int i;
				for (i = 0; i < stringListCount (identifiers); i++)
					newTarget (stringListItem (identifiers, i));
				stringListClear (identifiers);
				in_rule = true;
			}
		}
		else if (variable_possible && c == '=' &&
				 stringListCount (identifiers) == 1)
		{
			newMacro (stringListItem (identifiers, 0), false, appending, client, data);
			in_value = true;
			in_rule = false;
			appending = false;
		}
		else if (variable_possible && isIdentifier (c))
		{
			vString *name = vStringNew ();
			readIdentifier (c, name);
			stringListAdd (identifiers, name);

			if (in_value && client->valuesFound)
				client->valuesFound (client, name, data);

			if (stringListCount (identifiers) == 1)
			{
				if (in_define && ! strcmp (vStringValue (name), "endef"))
					in_define = false;
				else if (in_define)
					skipLine ();
				else if (! strcmp (vStringValue (name), "define"))
				{
					in_define = true;
					c = skipToNonWhite (nextChar ());
					vStringClear (name);
					/* all remaining characters on the line are the name -- even spaces */
					while (c != EOF && c != '\n')
					{
						vStringPut (name, c);
						c = nextChar ();
					}
					if (c == '\n')
						ungetcToInputFile (c);
					vStringStripTrailing (name);
					newMacro (name, true, false, client, data);
				}
				else if (! strcmp (vStringValue (name), "export"))
					stringListClear (identifiers);
				else if (! strcmp (vStringValue (name), "include")
					 || ! strcmp (vStringValue (name), "sinclude")
					 || ! strcmp (vStringValue (name), "-include"))
				{
					bool optional = (vStringValue (name)[0] == 'i')? false: true;
					while (1)
					{
						c = skipToNonWhite (nextChar ());
						readIdentifier (c, name);
						vStringStripTrailing (name);
						if (isAcceptableAsInclude(name))
							newInclude (name, optional);

						/* non-space characters after readIdentifier() may
						 * be rejected by the function:
						 * e.g.
						 * include $*
						 *
						 * Here, remove such characters from input stream.
						 */
						do
							c = nextChar ();
						while (c != EOF && c != '\n' && (!isspace (c)));
						if (c == '\n')
							ungetcToInputFile (c);

						if (c == EOF || c == '\n')
							break;
					}
				}
				else
				{
					if (client->directiveFound)
						client->directiveFound (client, name, data);

				}
			}
		}
		else
			variable_possible = false;
	}
	stringListDelete (identifiers);
}

static void findMakeTags (void)
{
	struct makeParserClient client = {
		.valuesFound    = NULL,
		.directiveFound = NULL,
		.newMacro = NULL,
	};
	runMakeParser (&client, NULL);
}


extern parserDefinition* MakefileParser (void)
{
	static const char *const patterns [] = { "[Mm]akefile", "GNUmakefile", NULL };
	static const char *const extensions [] = { "mak", "mk", NULL };
	parserDefinition* const def = parserNew ("Make");
	def->kinds      = MakeKinds;
	def->kindCount  = ARRAY_SIZE (MakeKinds);
	def->patterns   = patterns;
	def->extensions = extensions;
	def->parser     = findMakeTags;
	return def;
}
