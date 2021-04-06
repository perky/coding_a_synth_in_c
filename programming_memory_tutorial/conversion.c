#include <stdio.h>

int main()
{   
    unsigned char REG_A = 1;
    unsigned char REG_B = 0;
    unsigned char REG_C = 0;
    unsigned char REG_D = 0;
    unsigned char cells[72];
    REG_C = 11;
    REG_B = REG_A;

    while( REG_A > 0 )
    {
        cells[REG_A] = REG_A;
        REG_A += REG_B;
        if (REG_A == REG_C)
        {
            break;
        }
    }

    printf("%i, %i, %i, %i", cells[1], cells[2], cells[3], cells[4]);
    
    // the end.
    return 0;
}