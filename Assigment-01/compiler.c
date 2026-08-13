#include <stdio.h>
#include "compiler.h"
#include <string.h>

void compile(){
    char line[100],op;
    int dest, src1, src2, value;
    unsigned char instruction[4];

    FILE *fp_input = fopen("input.txt","r");
    
    if(fp_input==NULL)return;
    FILE *fp_output = fopen("program.byte","wb");
    if(fp_output==NULL)return;
    while(fgets(line,sizeof(line),fp_input)){
        if(strncmp(line,"Read",4)==0){
            sscanf(line,"Read x%d,%d",&dest,&src1);
            instruction[0]=5;
            instruction[1]=dest;
            instruction[2]=src1;
            instruction[3]=0;

            fwrite(instruction,sizeof(unsigned char),4,fp_output);
        }else if(strncmp(line,"Write",5)==0){
            sscanf(line,"Write x%d,%d",&dest,&src1);
            instruction[0]=6;
            instruction[1]=dest;
            instruction[2]=src1;
            instruction[3]=0;
            fwrite(instruction,sizeof(unsigned char),4,fp_output);
        }else if(sscanf(line,"x%d = x%d %c x%d",&dest,&src1,&op,&src2)==4){
            switch (op)
            {
                case '+':
                    instruction[0] = 1;
                    break;

                case '-':
                    instruction[0] = 2;
                    break;

                case '*':
                    instruction[0] = 3;
                    break;

                case '/':
                    instruction[0] = 4;
                    break;

                default:
                    continue;
            }
            instruction[1] = dest;
            instruction[2] = src1;
            instruction[3] = src2;
            fwrite(instruction,sizeof(unsigned char),4,fp_output);
        }else if(sscanf(line,"x%d = %d",&dest,&value)==2){
            instruction[0]=7;
            instruction[1]=dest;
            instruction[2]=value;
            instruction[3]=0;
            fwrite(instruction, sizeof(unsigned char), 4, fp_output);

        }
    }
    instruction[0] = 0;
    instruction[1] = 0;
    instruction[2] = 0;
    instruction[3] = 0;
    fwrite(instruction, sizeof(unsigned char), 4, fp_output);
    fclose(fp_input);
    fclose(fp_output);
}
