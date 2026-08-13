#include <stdio.h>

int main()
{
    FILE *fp = fopen("program.byte", "rb");

    unsigned char Instruction[4];

    while (fread(Instruction, 1, 4, fp) == 4)
    {
        printf("%d %d %d %d\n",
               Instruction[0],
               Instruction[1],
               Instruction[2],
               Instruction[3]);
    }

    fclose(fp);

    return 0;}