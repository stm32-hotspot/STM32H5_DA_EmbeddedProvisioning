#ifndef PRODUCT_STATE_H
#define PRODUCT_STATE_H
#include "main.h"

void ProductState_Set(uint32_t prodState);
uint32_t ProductState_Get(void);
void ProductState_Close(void);
void ProductState_Regression(void);

#endif
