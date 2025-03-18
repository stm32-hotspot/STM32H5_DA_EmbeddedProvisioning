#include "product_state.h"

#ifdef DEBUG
typedef struct
{
	uint8_t id;
	char name[15];
}sProdState;

sProdState ProdStates[] = {
		{ 0xED, "OPEN" },
		{ 0x17, "PROVISIONING" },
		{ 0x17, "PROVISIONING" },
		{ 0x2E, "PROVISIONED" },
		{ 0xC6, "TZ-CLOSED" },
		{ 0x72, "CLOSED" },
		{ 0x5C, "LOCKED" }
};
#endif

void ProductState_Set(uint32_t prodState)
{
  FLASH_OBProgramInitTypeDef flash_option_bytes_bank1 = {0};
  HAL_StatusTypeDef ret = HAL_ERROR;

  PRINTF("Setting product state to 0x%lx ...\r\n", prodState >> FLASH_OPTSR_PRODUCT_STATE_Pos);

   /* Unlock the Flash to enable the flash control register access */
  HAL_FLASH_Unlock();

  /* Unlock the Options Bytes */
  HAL_FLASH_OB_Unlock();

  flash_option_bytes_bank1.OptionType = OPTIONBYTE_PROD_STATE;
  flash_option_bytes_bank1.ProductState = prodState;

  PRINTF("Program state ...\r\n");

  ret = HAL_FLASHEx_OBProgram(&flash_option_bytes_bank1);
  if (ret != HAL_OK)
  {
	  printf("Error while setting OB Bank1 config state %ld : %d\r\n", prodState, ret);
	  while(1);
  }

  PRINTF("OB Launch ...\r\n");

  /* Launch the Options Bytes (reset the board, should not return) */
  ret = HAL_FLASH_OB_Launch();
  if (ret != HAL_OK)
  {
    printf("Error while execution OB_Launch : %d\r\n", ret);
    while(1);
  }
}


uint32_t ProductState_Get(void)
{
	uint32_t productState= (FLASH->OPTSR_CUR & FLASH_OPTSR_PRODUCT_STATE_Msk) >> FLASH_OPTSR_PRODUCT_STATE_Pos;
#ifdef DEBUG
	for (uint32_t i=0; i< (sizeof (ProdStates) / sizeof (sProdState)); i++)
	{
		if (productState == ProdStates[i].id)
		{
			printf("PRODUCT_STATE : %s\r\n", ProdStates[i].name);
			return productState;
		}
	}

	printf("Unknown PRODUCT_STATE : 0x%lx\r\n", productState);
    return 0;
#else
    return productState;
#endif
}

void ProductState_Close(void)
{
	PRINTF("Close device. Check not already closed\r\n");
	if ((FLASH->OPTSR_CUR & FLASH_OPTSR_PRODUCT_STATE_Msk) == OB_PROD_STATE_CLOSED)
	{
		PRINTF("Device Already closed\r\n");
		return;
	}

	// Important : if BOOT_UBE is set to 0xC3 the device will boot on STiRoT when closed
	// Here provisioning of only for OEMiRoT case
	PRINTF("Close device. Check BOOT_UBE OB set for Flash boot\r\n");
	if ((FLASH->OPTSR_CUR & FLASH_OPTSR_BOOT_UBE_Msk) != OB_UBE_OEM_IROT)
	{
		printf("Boot UBE not set properly : 0x%lx\r\n", (FLASH->OPTSR_CUR & FLASH_OPTSR_BOOT_UBE_Msk) >> FLASH_OPTSR_BOOT_UBE_Pos);
		return;
	}
	else
	{
		PRINTF("Boot UBE set properly : 0x%lx\r\n", (FLASH->OPTSR_CUR & FLASH_OPTSR_BOOT_UBE_Msk)  >> FLASH_OPTSR_BOOT_UBE_Pos);
	}

	PRINTF("Move to iROT Provisioned ...\r\n");
	ProductState_Set(OB_PROD_STATE_IROT_PROVISIONED);
	PRINTF("Move to Closed ...\r\n");
	ProductState_Set(OB_PROD_STATE_CLOSED);
	PRINTF("Reset ...\r\n");
	NVIC_SystemReset();
}

void ProductState_Regression(void)
{
	PRINTF("Launching regression ...\r\n");
	ProductState_Set(OB_PROD_STATE_REGRESSION);
	PRINTF("Reset ...\r\n");
	NVIC_SystemReset();
}
