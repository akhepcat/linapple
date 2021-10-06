#if(! HAS_STRLCPY)

#include <cstring>
#define __restrict // ****

std::size_t
__l_strlcpy(char * __restrict dst, const char * __restrict src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

//	memset(d,'\0',siz); // set all of the destination to zero

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (n-- != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1 );	/* count does  include NUL */
}


#endif
