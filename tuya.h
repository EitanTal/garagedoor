#pragma once
#include <stdbool.h>

void RxTask(void);
void TxTask(void);
void UartSetup(void);
void StatusReport(bool isOpen, uint8_t dpid);
void WifiReset(uint8_t mode);

extern bool RxCommand_open;
extern bool RxCommand_close;
extern bool wifiResetInProgress;
