#include <string.h>

void custom_spread(unsigned char *output, unsigned char *input, int len)
{
    memcpy(output, input, len);
}
