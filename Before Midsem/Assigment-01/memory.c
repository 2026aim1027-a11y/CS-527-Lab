#include <stdio.h>
#include <string.h>
#include "memory.h"

unsigned char Instruction[256];
unsigned char Data[256];

void initialise(){
    memset(Instruction, 0, sizeof(Instruction));
    memset(Data, 0, sizeof(Data));
    
    FILE *fp_program = fopen("program.byte","rb");
    if(fp_program==NULL)return;
    fread(Instruction,sizeof(unsigned char),256,fp_program);
    fclose(fp_program);

    FILE *fp_data  = fopen("data.byte","rb");
    if(fp_data==NULL)return;
    fread(Data,sizeof(unsigned char),256,fp_data);
    fclose(fp_data);
}
void finalize(){
    FILE *fp = fopen("data.byte","wb");
    if(fp == NULL)return;
    fwrite(Data,sizeof(unsigned char),256,fp);
    fclose(fp);
}
