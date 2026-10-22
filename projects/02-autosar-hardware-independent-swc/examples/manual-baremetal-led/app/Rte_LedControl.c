#include<stdio.h>
#include"Led_Hw.h"

#include"Rte_LedControl.h"

void Rte_Write_LedCommand(bool value){
    Led_Hw_Write(value);
}