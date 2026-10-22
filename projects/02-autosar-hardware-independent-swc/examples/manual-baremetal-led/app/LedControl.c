#include<stdio.h>
#include<stdbool.h>
#include"Rte_LedControl.h"
 typedef struct{
    bool UnitDelay_DSTATE;
 }ledControl_data;

static ledControl_data ledData;

void LedControl_Init(void){
    ledData.UnitDelay_DSTATE = false;
}

void LedControl_Step(void){
    bool ledCommand;
    ledCommand = ledData.UnitDelay_DSTATE;
    Rte_Write_LedCommand(ledCommand);
    ledData.UnitDelay_DSTATE = !ledData.UnitDelay_DSTATE;

}
