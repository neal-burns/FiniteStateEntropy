#include <string.h>
#include <stdlib.h>

void custom_spread(unsigned char *output, unsigned char *sorted_symbols, int len)
{
    int i;

    for(i=0; i<len-1; i++) {
        unsigned char c;
        int j = random() % (len - i);
        c = sorted_symbols[j];
        sorted_symbols[j] = sorted_symbols[i];
        sorted_symbols[i] = c;
    }

    memcpy(output, sorted_symbols, len);
}
