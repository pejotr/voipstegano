#include <iostream>
#include <stdio.h>
#include "voipsteg/algo/double.hxx"


int main()
{
    char testdata[1] = { 'a' };
    unsigned char *encoded;

    algo_double_init();

    algo_double_encoder(testdata, 1, &encoded);
    printf("%x ", encoded[0]);
    printf("%x\n", encoded[1]);

    unsigned char *decoded;
    algo_double_decoder((char*)encoded, &decoded);
    printf("%x\n", *decoded);


}
