/***************************************************************************
 *  Pinfo is a ncurses based lynx style info documentation browser
 *
 *  Copyright (C) 1999  Przemek Borys <pborys@dione.ids.pl>
 *  Copyright (C) 2005  Bas Zoetekouw <bas@debian.org>
 *  Copyright 2005  Nathanael Nerode <neroden@gcc.gnu.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 ***************************************************************************/

#include "common_includes.h"
#include <string>
using std::string;
RCSID("$Id$")

typedef struct
{
	char *suffix;
	char *command;
}
Suffixes;

void
basename_and_dirname(string filename, string& basename, string& dirname)
{
	/* Dirname should end with a slash, or be empty. */
	string::size_type index = filename.rfind('/');
	if (index == string::npos) {
		basename = filename;
		dirname = "";
	} else {
		basename = filename.substr(index + 1);
		dirname = filename.substr(0, index + 1);
	}
}

void
basename(string filename, string& basename_str)
{
	string::size_type index = filename.rfind('/');
	if (index == string::npos) {
		basename_str = filename;
	} else {
		basename_str = filename.substr(index + 1);
	}
}


/******************************************************************************
 * This piece of declarations says what to do with info files stored with      *
 * different formats/compression methods, before putting them into a temporary *
 * file. I.e. you don't do anything to plain `.info' suffix; for a `.info.gz'  *
 * you dump the file through `gunzip -d -c', etc.                              *
 ******************************************************************************/

#define SuffixesNumber 4

Suffixes suffixes[SuffixesNumber] =
{
	{"", 		"cat"},
	{".gz",		"gzip -d -q -c"},
	{".Z",		"gzip -d -q -c"},
	{".bz2",	"bzip2 -d -c"}
};

/*****************************************************************************/

char **infopaths = 0;
int infopathcount = 0;

int
qsort_cmp(const void *base, const void *compared)
{
	char *cbase =((TagTable *) base)->nodename;
	char *ccompared =((TagTable *) compared)->nodename;
	return compare_tag_table_string(cbase, ccompared);
}

int
matchfile(char **buf, const string name_string)
{
#define Buf	(*buf)
	DIR *dir;
	string basename_string;
	string dirname_string;
	basename_and_dirname(name_string, basename_string, dirname_string);

	const char *bname = basename_string.c_str();
	struct dirent *dp;
	int namelen = strlen(bname);
	int matched = 0;
	if (Buf[strlen(Buf)-1]!='/')
		strcat(Buf,"/");
	strcat(Buf,dirname_string.c_str());
	dir = opendir(Buf);	/* here we always have '/' at end */
	if (dir == NULL)
		return 1;
	while ((dp = readdir(dir)) != NULL)
	{
		if (strncmp(dp->d_name, bname, namelen) == 0)
		{
			string tmp_string = dp->d_name;
			strip_compression_suffix(tmp_string);

			char *tmp = strdup(tmp_string.c_str());
			int dl;
			dl = strlen(tmp);
			if ((!isdigit(tmp[dl - 1])) &&(!isalpha(tmp[namelen])))
				/* if it's not eg. info-2.gz, but info.gz, the primary page
				 * && it's not a different name(eg. gdbm instead gdb) */
			{
				if ((!matched) ||(strlen(tmp) < matched))
				{
					Buf[strlen(Buf) - matched - 1] = '\0';
					strcat(Buf, "/");
					strcat(Buf, tmp);
					matched = strlen(tmp);
				}
			}
		}
	}
	closedir(dir);
	if (matched)
		return 0;
	return 1;
#undef Buf
}

FILE *
dirpage_lookup(char **type, char ***message, long *lines,
		char *filename, char **first_node)
{
#define Type	(*type)
#define Message	(*message)
#define Lines	(*lines)
	FILE *id = 0;
	int filenamelen = strlen(filename);
	int goodHit = 0, perfectHit = 0;
	char name[256];
	char file[256];
	int i;
	id = opendirfile(0);
	if (!id)
		return 0;
	read_item(id, type, message, lines);
	for (i = 1; i < Lines; i++)	/* initialize node-links for every line */
	{
		if ((Message[i][0] == '*') &&(Message[i][1] == ' ') &&(!perfectHit))
		{
			char *nameend = strchr(Message[i], ':');
			if (nameend)
			{
				if (*(nameend + 1) != ':')	/* form: `* name:(file)node.' */
				{
					char *filestart = strchr(nameend, '(');
					if (filestart)
					{
						char *fileend = strchr(filestart, ')');
						if (fileend)
						{
							char *dot = strchr(fileend, '.');
							if (dot)
							{
								if (strncmp(filename, Message[i] + 2, filenamelen) == 0)
								{
									char *tmp = name;
									strncpy(file, filestart + 1, fileend - filestart - 1);
									file[fileend - filestart - 1] = 0;
									strncpy(name, fileend + 1, dot - fileend - 1);
									name[dot - fileend - 1] = 0;
									while (isspace(*tmp))
										tmp++;
									if (strlen(name))
									{
										*first_node = (char*)xmalloc(strlen(tmp) + 1);
										strcpy((*first_node), tmp);
									}
									if (id)
										fclose(id);	/* we don't need dirfile/badly matched infofile open anymore */
									id = 0;
									if (!strstr(file, ".info"))
										strcat(file, ".info");
									id = openinfo(file, 0);
									goodHit = 1;
									if ((nameend - Message[i]) - 2 == filenamelen)	/* the name matches perfectly to the query */
										perfectHit = 1;	/* stop searching for another matches, and use this one */
								}
							}
						}
					}
				}
			}
		}
	}
	if (!goodHit)
	{
		fclose(id);
		id = 0;
	}
	return id;
#undef Lines
#undef Message
#undef Type
}

void
freeitem(char **type, char ***buf, long *lines)
{
#define Type	(*type)
#define Buf		(*buf)
#define Lines	(*lines)
	long i;

	if (Type != 0)
	{
		xfree(Type);
		Type = 0;
	}
	if (Buf != 0)
	{
		for (i = 1; i <= Lines; i++)
			if (Buf[i] != 0)
			{
				xfree(Buf[i]);
				Buf[i] = 0;
			}
		xfree(Buf);
		Buf = 0;
	}
#undef Type
#undef Buf
#undef Lines
}

void
read_item(FILE * id, char **type, char ***buf, long *lines)
{

#define Type	(*type)
#define Buf		(*buf)
#define Lines	(*lines)

	freeitem(type, buf, lines);	/* free previously allocated memory */

	while (fgetc(id) != INFO_TAG);	/*
									 * seek precisely on the INFO_TAG
									 *(the seeknode function may be
									 * imprecise in combination with
									 * some weird tag_tables).
									 */
	while (fgetc(id) != '\n');	/* then skip the trailing `\n' */

	Type = (char*)xmalloc(1024);	/* read the header line */
	fgets(Type, 1024, id);
	Type = (char*)xrealloc(Type, strlen(Type) + 1);
	Lines = 0;			/* set number of lines to 0 */

	Buf = (char**)xmalloc(sizeof(char **));	/* initial buffer allocation */
	do
	{
		if (feof(id))		/* don't read after eof in info file */
			break;
		if (Lines)		/* make a reallocation for new input line */
		{
			Buf[Lines] = (char*)xrealloc(Buf[Lines], strlen(Buf[Lines]) + 1);
		}
		Lines++;			/* increase the read lines number */

		Buf = (char**)xrealloc(Buf, sizeof(char **) *(Lines + 1));
		Buf[Lines] = (char*)xmalloc(1024);
		Buf[Lines][0] = 0;

		if (fgets(Buf[Lines], 1024, id) == NULL)		/*
														 * if the line was not found
														 * in input file,
														 * fill the allocated space
														 * with empty line.
														 */
			strcpy(Buf[Lines], "\n");
	}
	while (Buf[Lines][0] != INFO_TAG);	/* repeat until new node mark is found */


	if (Lines)			/* added for simplifing two-line ismenu and isnote functs */
	{
		strcpy(Buf[Lines], "\n");
		Buf[Lines] = (char*)xrealloc(Buf[Lines], strlen(Buf[Lines]) + 1);
	}

	fseek(id, -2, SEEK_CUR);
#undef Type
#undef Buf
#undef Lines

}
void
load_indirect(char **message, long lines)
{
	int cut = 0;			/* number of invalid entries */
	indirect = (Indirect*)xmalloc((lines + 1) * sizeof(Indirect));
	for (long i = 1; i < lines; i++) {
		string wsk_string = message[i];
		unsigned int n = 0;
		/* Find the first colon, but not in position 0 */
		n = wsk_string.find(':', 1);
		if (n == string::npos) {
			/* No colon.  Invalid entry. */
			cut++;			/* if the entry was invalid, make indirect count shorter */
		} else {
			string filename;
			filename = wsk_string.substr(0, n);
			strncpy(indirect[i - cut].filename, filename.c_str(), 200);

			string remainder;
			remainder = wsk_string.substr(n + 2, string::npos);
			indirect[i - cut].offset = atoi(remainder.c_str());
		}
	}
	IndirectEntries = lines - 1 - cut;
}

void
load_tag_table(char **message, long lines)
{
	long i;
	char *wsk, *wsk1;
	int is_indirect = 0;
	register unsigned int j;
	register char *res;
	int cut = 0;			/* holds the number of corrupt lines */

	/*
	 * if in the first line there is a(indirect) string, skip that line
	 * by adding the value of is_indirect=1 to all message[line] references.
	 */
	if (strcasecmp("(Indirect)", message[1]) == 0)
		is_indirect = 1;
	tag_table = (TagTable*)xmalloc((lines + 1) * sizeof(TagTable));
	for (i = 1; i < lines - is_indirect; i++)
	{
		char *check;
		wsk = message[i + is_indirect];
		check = wsk + strlen(wsk);
		while (!isspace(*(++wsk)))
		{
			if (wsk >= check)
			{
				wsk--;
				break;
			}
		}
		wsk++;
		wsk1 = wsk;
		check = wsk1 + strlen(wsk1);
		while (*(++wsk1) != INDIRECT_TAG)
		{
			if (wsk1 >= check)
				break;
		}
		if (wsk1 < check)
		{
			(*wsk1) = 0;
			/*
			 * original: sprintf(tag_table[i-cut].nodename,"%s",wsk);
			 * below is a faster version.
			 */
			res = (char*)memcpy(tag_table[i - cut].nodename, wsk, j =(size_t)(wsk1 - wsk));
			(*(res += j + 1)) = 0;
			(*wsk1) = INDIRECT_TAG;
			wsk1++;
			tag_table[i - cut].offset = atoi(wsk1);
		}
		else
			cut++;			/* increment the number of corrupt entries */
	}
	TagTableEntries = lines - 1 - is_indirect - cut;

	/* FIXME: info should ALWAYS start at the 'Top' node, not at the first
	   mentioned node(vide ocaml.info) */

	for (i = 1; i <= TagTableEntries; i++)
	{
		if (strcasecmp(tag_table[i].nodename, "Top") == 0)
		{
			FirstNodeOffset = tag_table[i].offset;
			strcpy(FirstNodeName, tag_table[i].nodename);
		}
	}
	qsort(&tag_table[1], TagTableEntries, sizeof(TagTable), qsort_cmp);
}

int
seek_indirect(FILE * id)
{
	int finito = 0;
	long seek_pos;
	int input;
	char *type = (char*)xmalloc(1024);
	fseek(id, 0, SEEK_SET);
	while (!finito)		/*
						 * scan through the file, searching for "indirect:"
						 * string in the type(header) line of node.
						 */
	{
		while ((input = fgetc(id)) != INFO_TAG)
			if (input == EOF)
			{
				if (type)
				{
					xfree(type);
					type = 0;
				}
				return 0;
			}
		seek_pos = ftell(id) - 2;
		fgetc(id);
		fgets(type, 1024, id);
		if (strncasecmp("Indirect:", type, strlen("Indirect:")) == 0)
		{
			finito = 1;
		}
	}
	xfree(type);
	type = 0;
	if (!curses_open)
		printf(_("Searching for indirect done\n"));
	else
	{
		attrset(bottomline);
		mvhline(maxy - 1, 0, ' ', maxx);
		mvaddstr(maxy - 1, 0, _("Searching for indirect done"));
		attrset(normal);
	}
	fseek(id, seek_pos, SEEK_SET);
	return 1;
}

/*
 * second arg for dumping out verbose debug info or not :)
 */
int
seek_tag_table(FILE * id,int quiet)
{
	int finito = 0;
	long seek_pos;
	int input;
	char *type = (char*)xmalloc(1024);
	fseek(id, 0, SEEK_SET);
	/*
	 * Scan through the file, searching for a string
	 * "Tag Table:" in the type(header) line of node.
	 */
	while (!finito)
	{
		while ((input = fgetc(id)) != INFO_TAG)
		{
			if (input == EOF)
			{
				if (!quiet)
				{
					if (!curses_open)
						printf(_("Warning: could not find tag table\n"));
					else
					{
						attrset(bottomline);
						mvhline(maxy - 1, 0, ' ', maxx);
						mvaddstr(maxy - 1, 0, _("Warning: could not find tag table"));
						attrset(normal);
					}
				}
				if (type)
				{
					xfree(type);
					type = 0;
				}
				return 2;
			}
		}
		seek_pos = ftell(id) - 2;
		while (fgetc(id) != '\n')
		{
			if (feof(id))
				break;
		}
		fgets(type, 1024, id);
		if (strncasecmp("Tag Table:", type, strlen("Tag Table:")) == 0)
		{
			finito = 1;
		}
	}
	xfree(type);
	type = 0;
	if (!curses_open)
		printf(_("Searching for tag table done\n"));
	else
	{
		attrset(bottomline);
		mvhline(maxy - 1, 0, ' ', maxx);
		mvaddstr(maxy - 1, 0, "Searching for tag table done");
		attrset(normal);
	}
	fseek(id, seek_pos, SEEK_SET);
	return 1;
}

FILE *
opendirfile(int number)
{
	FILE *id = NULL;
	string bufstr;
	int i;
	char *tmpfilename;
	int *fileendentries = (int*)xmalloc(infopathcount * sizeof(int));
	int dir_found = 0;
	int dircount = 0;
	int lang_found;
	struct stat status;

	if (number == 0)		/* initialize tmp filename for file 1 */
	{
		if (tmpfilename1)
		{
			unlink(tmpfilename1);	/* erase old tmpfile */
			free(tmpfilename1);
		}
		tmpfilename1 = tempnam("/tmp", NULL);
		tmpfilename = tmpfilename1;	/* later we will refere only to tmp1 */
	}
	for (i = 0; i < infopathcount; i++)	/* go through all paths */
	{
		lang_found = 0;
		bufstr = infopaths[i];
		bufstr += '/';

		char* getenv_lang;
		getenv_lang = getenv("LANG");
		if (getenv_lang != NULL)
			bufstr += getenv_lang;
		bufstr += "/dir";

		/* FIXME: Duplicated code */
		for (int j = 0; j < SuffixesNumber; j++)	/* go through all suffixes */
		{
			string bufstr_with_suffix;
			bufstr_with_suffix = bufstr;
			bufstr_with_suffix += suffixes[j].suffix;

			id = fopen(bufstr_with_suffix.c_str(), "r");
			if (id != NULL) {
				fclose(id);
				/* FIXME: Insecure temp file usage */
				string command_string = suffixes[j].command;
				command_string += " ";
				command_string += bufstr_with_suffix;
				command_string += ">> ";
				command_string += tmpfilename;
				system(command_string.c_str());
				lstat(tmpfilename, &status);
				fileendentries[dircount] = status.st_size;
				dircount++;
				dir_found = 1;
				lang_found = 1;
			}
		}

		/* same as above, but without $LANG support */
		if (!lang_found)
		{
			bufstr = infopaths[i];
			bufstr += "/dir";
			/* FIXME: Duplicated code */
			for (int j = 0; j < SuffixesNumber; j++)	/* go through all suffixes */
			{
				string bufstr_with_suffix;
				bufstr_with_suffix += suffixes[j].suffix;
				id = fopen(bufstr_with_suffix.c_str(), "r");
				if (id != NULL) {
					fclose(id);
					/* FIXME: Insecure temp file usage */
					string command_string = suffixes[j].command;
					command_string += " ";
					command_string += bufstr_with_suffix;
					command_string += ">> ";
					command_string += tmpfilename;
					system(command_string.c_str());
					lstat(tmpfilename, &status);
					fileendentries[dircount] = status.st_size;
					dircount++;
					dir_found = 1;
				}
			}
		}
	}
	if (dir_found)
		id = fopen(tmpfilename, "r");
	/*
	 * Filter the concatenated dir pages to exclude hidden parts of info
	 * entries
	 */
	if (id)
	{
		char *tmp;
		long filelen, i;
		int aswitch = 0;
		int firstswitch = 0;
		dircount = 0;

		fseek(id, 0, SEEK_END);
		filelen = ftell(id);

		tmp = (char*)xmalloc(filelen);
		fseek(id, 0, SEEK_SET);
		fread(tmp, 1, filelen, id);
		fclose(id);
		id = fopen(tmpfilename, "w");
		for (i = 0; i < filelen; i++)
		{
			if (tmp[i] == INFO_TAG)
			{
				aswitch ^= 1;
				if (!firstswitch)
					fputc(tmp[i], id);
				firstswitch = 1;
			}
			else if ((aswitch) ||(!firstswitch))
				fputc(tmp[i], id);
			if (i + 1 == fileendentries[dircount])
			{
				if (aswitch != 0)
					aswitch = 0;
				dircount++;	/* the last dircount should fit to the end of filelen */
			}
		}
		fputc(INFO_TAG, id);
		fputc('\n', id);
		xfree(fileendentries);
		fclose(id);
		id = fopen(tmpfilename, "r");
		xfree(tmp);

		return id;
	}
	return NULL;
}

/*
 * Note: openinfo is a function for reading info files, and putting
 * uncompressed content into a temporary filename.  For a flexibility, there
 * are two temporary files supported, i.e.  one for keeping opened info file,
 * and second for i.e. regexp search across info nodes, which are in other
 * info-subfiles.  The temporary file 1 is refrenced by number=0, and file 2 by
 * number=1 Openinfo by default first tries the path stored in char
 * *filenameprefix and then in the rest of userdefined paths.
 */
FILE *
openinfo(const char *filename, int number)
{
	FILE *id = NULL;
	char *buf = (char*) xmalloc(1024);	/* holds local copy of filename */
	char *bufend;			/* points at the trailing 0 of initial name */
	int i, j;
	char *tmpfilename;

	if (strncmp(filename, "dir", 3) == 0)
	{
		xfree(buf);
		return opendirfile(number);
	}

	if (number == 0)		/* initialize tmp filename for file 1 */
	{
		if (tmpfilename1)
		{
			unlink(tmpfilename1);	/* erase old tmpfile */
			free(tmpfilename1);
		}
		tmpfilename1 = tempnam("/tmp", NULL);
		tmpfilename = tmpfilename1;	/* later we will refere only to tmp1 */
	}
	else /* initialize tmp filename for file 2 */
	{
		if (tmpfilename2)
		{
			unlink(tmpfilename2);	/* erase old tmpfile */
			free(tmpfilename2);
		}
		tmpfilename2 = tempnam("/tmp", NULL);
		tmpfilename = tmpfilename2;	/* later we will refere only to tmp2 */
	}

	for (i = -1; i < infopathcount; i++)	/* go through all paths */
	{
		if (i == -1)
		{
			/*
			 * no filenameprefix, we don't navigate around any specific
			 * infopage set, so simply scan all directories for a hit
			 */
			if (!filenameprefix)
				continue;
			else
			{
				strcpy(buf, filenameprefix);	/* build a filename */
				strcat(buf, "/");
				string filename_string = filename;
				string basename_string;
				basename(filename_string, basename_string);
				strcat(buf, basename_string.c_str());
			}
		}
		else
		{
			strcpy(buf, infopaths[i]);	/* build a filename */
			string filename_string = filename;
			if (matchfile(&buf, filename_string) == 1)	/* no match found in this directory */
				continue;
		}
		bufend = buf;
		/*
		 * remember the bufend to make it possible later to glue compression
		 * suffixes.
		 */
		bufend += strlen(buf);
		for (j = 0; j < SuffixesNumber; j++)	/* go through all suffixes */
		{
			strcat(buf, suffixes[j].suffix);
			if ((id = fopen(buf, "r")) != NULL)
			{
				fclose(id);
				clearfilenameprefix();
				filenameprefix = strdup(buf);
				{			/* small scope for removal of filename */
					int prefixi, prefixlen = strlen(filenameprefix);
					for (prefixi = prefixlen; prefixi > 0; prefixi--)
						if (filenameprefix[prefixi] == '/')
						{
							filenameprefix[prefixi] = 0;
							break;
						}
				}
				/* FIXME: Insecure temp file usage */
				string command_string = suffixes[j].command;
				command_string += ' ';
				command_string += buf;
				command_string += "> ";
				command_string += tmpfilename;
				system(command_string.c_str());

				id = fopen(tmpfilename, "r");
				if (id)
				{
					xfree(buf);
					return id;
				}
			}
			(*bufend) = 0;
		}
		if ((i == -1) &&(filenameprefix))	/* if we have a nonzero filename prefix,
											   that is we view a set of infopages,
											   we don't want to search for a page
											   in all directories, but only in
											   the prefix directory. Therefore
											   break here. */
			break;
	}
	xfree(buf);
	return 0;
}

void
addrawpath(const string filename_string)
{
	/* Get the portion up to the last slash. */
	string dirstring;
	string::size_type index = filename_string.rfind('/');
	if (index != string::npos)
		dirstring = filename_string.substr(0, index + 1);
	else
		dirstring = "./"; /* If no directory part, use current directory */

	infopaths = (char**)xrealloc(infopaths,(infopathcount + 3) *(sizeof(char *)));
	for (int i = infopathcount; i > 0; i--)	/* move entries to the right */
		infopaths[i] = infopaths[i - 1];

	infopaths[0]=strdup(dirstring.c_str());	/* add new(raw) entry */
	infopathcount++;
}

int
isininfopath(char *name)
{
	int i;
	for (i = 0; i < infopathcount; i++)
	{
		if (strcmp(name, infopaths[i]) == 0)
			return 1;		/* path already exists */
	}
	return 0;			/* path not found in previous links */
}

/* returns the number of chars ch in string str */
unsigned int
charcount(const char *str, const char ch)
{
	int num = 0;
	const char *c;

	c = str;
	
	while (*c != '\0')
	{
		if (*c++ == ch)
			num++;
	}
	return num;
}

/* 
 * find the paths where info files are to be found, 
 * and put them in the global var infopaths[]
 */
void
initpaths()
{
	char emptystr[1] = "";
	char **paths = NULL;
	char *infopath = NULL, *langpath = NULL;
	char *c, *dir, *env;
	char *rawlang = NULL, *lang = NULL, *langshort = NULL;
	int ret;
	unsigned int i, j, maxpaths, numpaths = 0, infolen, langlen;
	size_t len;
	struct stat sbuf;
	ino_t *inodes;

	/* first concat the paths */
	env = getenv("INFOPATH");
	if (env == NULL)
	{
		env = emptystr;
	}
	infolen = strlen(env) + strlen(configuredinfopath) + 2;
	infopath = (char *) xmalloc( infolen );
	strcat(infopath, env);
	strcat(infopath, ":");
	strcat(infopath, configuredinfopath);

	/* alloc the paths[] array */
	maxpaths = 3 * (charcount( infopath, ':' ) + 1); // *3 for $LANG
	paths = (char **) xmalloc( maxpaths * sizeof(char *) );

	/* split at ':' and put the path components into paths[] */
	c = infopath;
	while (dir = strsep(&c, ":"))
	{
		/* if this actually is a non-empty string, add it to paths[] */
		if ( dir && strlen(dir)>0 ) 
		{
			paths[numpaths++] = dir;
		}
	}

	/* get the current $LANG, if any (to use for localized info pages) */
	rawlang = getenv("LANG");
	if (rawlang) {
		lang = strdup(rawlang);
		/* fix the lang string */
		for (i=0; lang[i]!='\0'; i++)
		{
			/* cut off the charset */
			if (lang[i]=='.') 
			{
				lang[i]='\0';
			}
			/* if lang is sublocalized (nl_BE or so), also use short version */
			if (lang[i]=='_' && langshort==NULL)
			{
				langshort = strdup(lang);
				langshort[i] = '\0';
			}
		}
	}
	/* if we have a LANG defined, add paths with this lang to the paths[] */
	if (lang && strlen(lang)>0 )
	{
		/* crude upper limit */
		langlen = infolen + (strlen(lang)+2) * numpaths + 1;
		if (langshort!=NULL) langlen *= 2;
		langpath = (char *) xmalloc( langlen * sizeof(char) );

		c = langpath; 
		for (i=0; i<numpaths; i++)
		{
			/* TODO: check for negative return values of sprintf */
			len = sprintf(c, "%s/%s", paths[i], lang);
			/* add the lang specific dir at the beginning */
			paths[numpaths+i] = paths[i];
			paths[i] = c;

			c += len+1;
			
			if (langshort) 
			{
				/* TODO: check for negative return values of sprintf */
				len = sprintf(c, "%s/%s", paths[numpaths+i], langshort);
				/* add the lang specific dir at the beginning */
				paths[2*numpaths+i] = paths[numpaths+i];
				paths[numpaths+i] = c;

				c += len+1;
			}

		}
		numpaths *= (langshort?3:2);
	}

#ifdef ___DEBUG___
	/* for debugging */
	for (i=0; i<numpaths; i++)
		fprintf(stderr,"--> %s\n", paths[i]);
#endif

	/* ok, now we have all the (possibly) revelevant paths in paths[] */
	/* now loop over them, see if they are valid and if they are duplicates*/
	inodes = (ino_t *) xmalloc( maxpaths * sizeof(ino_t *) );
	numpaths = 0;
	len = 0;
	for (i=0; i< maxpaths; i++)
	{
		/* stat() the dir */
		ret = stat( paths[i], &sbuf);
		/* and see if it could be opened */
		if (ret < 0)
		{
#ifdef ___DEBUG___
			fprintf(stderr, "error while opening `%s': %s\n", 
					paths[i], strerror(errno) );
#endif 
			paths[i] = NULL;
			inodes[i] = 0;
		}
		else
		{
			inodes[i] = sbuf.st_ino;
		}

		/* now check if this path is a duplicate */
		for (j=0; j<i; j++)
		{
			if (inodes[j]==inodes[i]) paths[i] = NULL;
		}

		/* calculate the total number of vali paths and the size of teh strings */
		if (paths[i]!=NULL) 
		{
			numpaths++;
			len += strlen(paths[i]) + 1;
		}
	}


	/* and alloc and copy to global var */
	infopathcount = numpaths;
	infopaths = (char **) xmalloc( numpaths * sizeof(char *) );
	c = (char *) xmalloc( len * sizeof(char) );
	j=0;
	for (i=0; i<maxpaths; i++)
	{
		if (paths[i]!=NULL)
		{
			/* copy path to c buffer */
			strcpy(c, paths[i]);	
			infopaths[j++] = c;
			c += strlen(paths[i]) + 1;
		}
	}
	

	xfree(infopath);
	xfree(langpath);
	xfree(paths);
	xfree(lang);
	xfree(langshort);
	xfree(inodes);

#ifdef ___DEBUG___
	/* for debugging */
	fprintf(stderr, "%i valid info paths found:\n", infopathcount);
	for (i=0; i<infopathcount; i++)
		if (infopaths[i]) fprintf(stderr,"--> %s\n", infopaths[i]);
#endif


}



	void
create_indirect_tag_table()
{
	FILE *id = 0;
	int i, j, initial;
	for (i = 1; i <= IndirectEntries; i++)
	{
		id = openinfo(indirect[i].filename, 1);
		initial = TagTableEntries + 1;
		if (id)
		{
			create_tag_table(id);
			FirstNodeOffset = tag_table[1].offset;
			strcpy(FirstNodeName, tag_table[1].nodename);
		}
		fclose(id);
		for (j = initial; j <= TagTableEntries; j++)
		{
			tag_table[j].offset +=(indirect[i].offset - FirstNodeOffset);
		}
	}
	FirstNodeOffset = tag_table[1].offset;
	strcpy(FirstNodeName, tag_table[1].nodename);
	qsort(&tag_table[1], TagTableEntries, sizeof(TagTable), qsort_cmp);
}
	void
create_tag_table(FILE * id)
{
	char *buf = (char*)xmalloc(1024);
	long oldpos;
	fseek(id, 0, SEEK_SET);
	if (!tag_table)
		tag_table = (TagTable*)xmalloc((TagTableEntries + 2) * sizeof(TagTable));
	else
		tag_table = (TagTable*)xrealloc(tag_table,(TagTableEntries + 2) * sizeof(TagTable));
	while (!feof(id))
	{
		if (fgetc(id) == INFO_TAG)	/* We've found a node entry! */
		{
			while (fgetc(id) != '\n');	/* skip '\n' */
			TagTableEntries++;	/* increase the nuber of tag table entries */
			oldpos = ftell(id);	/* remember this file position! */
			/*
			 * it is a an eof-fake-node (in some info files it happens, that
			 * the eof'ish end of node is additionaly signalised by an INFO_TAG
			 * We give to such node an unlike to meet nodename.
			 */
			if (fgets(buf, 1024, id) == NULL)
			{
				tag_table = (TagTable*)xrealloc(tag_table, sizeof(TagTable) *(TagTableEntries + 1));
				strcpy(tag_table[TagTableEntries].nodename, "12#!@#4");
				tag_table[TagTableEntries].offset = 0;
			}
			else
			{
				int colons = 0, i, j;
				int buflen = strlen(buf);
				for (i = 0; i < buflen; i++)
				{
					if (buf[i] == ':')
						colons++;
					if (colons == 2)	/*
										 * the string after the second colon
										 * holds the name of current node.
										 * The name may then end with `.',
										 * or with a newline, which is scanned
										 * bellow.
										 */
					{
						for (j = i + 2; j < buflen; j++)
						{
							if ((buf[j] == ',') ||(buf[j] == '\n'))
							{
								tag_table = (TagTable*)xrealloc(tag_table, sizeof(TagTable) *(TagTableEntries + 1));
								buf[j] = 0;
								buflen = j;
								strcpy(tag_table[TagTableEntries].nodename, buf + i + 2);
								tag_table[TagTableEntries].offset = oldpos - 2;
								break;
							}
						}
						break;
					}
				}		/* end: for loop, looking for second colon */
			}			/* end: not a fake node */
		}			/* end: we've found a node entry(INFO_TAG) */
	}				/* end: global while loop, looping until eof */
	xfree(buf);
	buf = 0;
	if (!indirect)
	{
		FirstNodeOffset = tag_table[1].offset;
		strcpy(FirstNodeName, tag_table[1].nodename);
		qsort(&tag_table[1], TagTableEntries, sizeof(TagTable), qsort_cmp);
	}
}

	void
seeknode(int tag_table_pos, FILE ** Id)
{
	int i;
#define id	(*Id)
	/*
	 * Indirect nodes are seeked using a formula:
	 * file-offset = tagtable_offset - indirect_offset +
	 *             + tagtable[1]_offset
	 */
	if (indirect)	
	{
		for (i = IndirectEntries; i >= 1; i--)
		{
			if (indirect[i].offset <= tag_table[tag_table_pos].offset)
			{
				long off = tag_table[tag_table_pos].offset - indirect[i].offset + FirstNodeOffset - 4;
				fclose(id);
				id = openinfo(indirect[i].filename, 0);
				if (id == NULL)
				{
					closeprogram();
					printf(_("Error: could not open info file\n"));
					exit(1);
				}
				if (off > 0)
					fseek(id, off, SEEK_SET);
				else
					fseek(id, off, SEEK_SET);
				break;
			}
		}
	}
	else
	{
		long off = tag_table[tag_table_pos].offset - 4;
		if (off > 0)
			fseek(id, off, SEEK_SET);
		else
			fseek(id, off, SEEK_SET);
	}
#undef id
}

/*
 * Strip one trailing .gz, .bz2, etc.
 * Operates in place.
 */
void
strip_compression_suffix(string& filename)
{
	for (int j = 0; j < SuffixesNumber; j++)
	{
		string::size_type suffix_len =  strlen(suffixes[j].suffix);
		if (    (filename.length() >= suffix_len)
		     && (filename.substr(filename.length() - suffix_len)
		         == suffixes[j].suffix)
		   ) {
			/* Truncate string. */
			filename.resize(filename.length() - suffix_len);
			break;
		}
	}
}

