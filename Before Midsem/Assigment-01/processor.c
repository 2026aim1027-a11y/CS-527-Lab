#include <stdio.h>
#include "processor.h"

int Register[256];
int PC, opcode, dest, src1, src2;
int end_of_simulation=0;

void reset(){
    for (int i=0;i<256;i++)Register[i]=0;
    PC=0;
    end_of_simulation=0;
}

void fetch(){
    opcode = Instruction[PC];
    dest = Instruction[PC+1];
    src1 = Instruction[PC+2];
    src2 = Instruction[PC+3];
    PC += 4;
}

void decode(){}

void execute(){
    switch (opcode)
    {
    case 0:
        end_of_simulation=1;
        break;
        
    case 1:
        Register[dest] = Register[src1] + Register[src2];
        break;

    case 2:
        Register[dest] = Register[src1] - Register[src2]; 
        break;
    case 3:
        Register[dest] = Register[src1] * Register[src2];
        break;
    case 4:
        if(Register[src2]!=0){
            Register[dest] = Register[src1] / Register[src2];
        }else{
            printf("Division by zero not possible ....");
        }
        break;
    case 5:
        Register[dest] = Data[src1];
        break;
    case 6:
        Data[src1] = Register[dest];
        break;
    case 7:
        Register[dest] = src1;
        break;   
    default:
    printf("Opcode not found");
    end_of_simulation=1;
    }
}