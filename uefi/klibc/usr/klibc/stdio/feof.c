#define __NO_STDIO_INLINES
#include "stdioint.h"

int feof(FILE *__f)
{
	return __f->_IO_eof;
}
