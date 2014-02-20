#include <string.h>

void custom_spread(unsigned char *output, unsigned char *sorted_symbols, int len)
{
    memcpy(output, sorted_symbols, len);
}
