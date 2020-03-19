#include "utils.h"


int wcsrep(wchar_t *str, const wchar_t *match, const wchar_t *rep)
{
	if (!str || !match)
		return -1;
	wchar_t tmpstr[BUFFER_SIZE];
	wchar_t *pos = wcsstr(str, match);
	if (pos)
	{
		memset(tmpstr, 0, sizeof(tmpstr));
		wcsncpy(tmpstr, str, pos - str);
		wcscat(tmpstr, rep);
		wcscat(tmpstr, pos + wcslen(match));
		wcscpy(str, tmpstr);
		return 0;
	}
	return -1;
}
