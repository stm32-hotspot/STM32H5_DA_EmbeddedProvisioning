#include "ob_trustzone.h"


void OBTrustZone_CheckAndSetTrustZone(void)
{
	FLASH_OBProgramInitTypeDef flash_option_bytes = {0};
	HAL_StatusTypeDef ret = HAL_ERROR;
	// Read current configuration
	HAL_FLASHEx_OBGetConfig(&flash_option_bytes);

	if((flash_option_bytes.USERConfig2 & FLASH_OPTSR2_TZEN) == OB_TZEN_ENABLE)
	{
		// Nothing to do
		PRINTF("TrustZone already enabled\r\n");
		return;
	}

	/* Unlock the Flash to enable the flash control register access */
	HAL_FLASH_Unlock();

	/* Unlock the Options Bytes */
	HAL_FLASH_OB_Unlock();

	flash_option_bytes.USERType = OB_USER_TZEN;
	flash_option_bytes.USERConfig2 = OB_TZEN_ENABLE;

	PRINTF("TZ Not enabled : Program TZEN option byte to 0xB4\r\n");
	ret = HAL_FLASHEx_OBProgram(&flash_option_bytes);
	if (ret != HAL_OK)
	{
		printf("Error while setting TrustZone : %d\r\n", ret);
		while(1);
	}
	ret=HAL_FLASH_OB_Launch();
	if (ret != HAL_OK)
	{
		printf("Error while execution OB_Launch\r\n");
		while(1);
	}
	// Reset to have TrustZone start
	PRINTF("Reset...\r\n\r\n");
	NVIC_SystemReset();
}

#define SECURE_WATERMARK_BANK1_START 0
#define SECURE_WATERMARK_BANK1_END   0x7F
#define SECURE_WATERMARK_BANK2_START 0x7F
#define SECURE_WATERMARK_BANK2_END   0


void OBTrustZone_CheckAndSetSecureWatermark(void)
{
	FLASH_OBProgramInitTypeDef flash_option_bytes = {0};
	HAL_StatusTypeDef ret = HAL_ERROR;
	uint32_t obUpdate=0;

	// Read current configuration
	flash_option_bytes.Banks = FLASH_BANK_1;
	HAL_FLASHEx_OBGetConfig(&flash_option_bytes);

	if ((flash_option_bytes.WMSecStartSector != SECURE_WATERMARK_BANK1_START)
	 || (flash_option_bytes.WMSecEndSector != SECURE_WATERMARK_BANK1_END))
	{
		PRINTF("Flash watermarks bank1 not set correctly : Start 0x%2.2lx End 0x%2.2lx\r\n", flash_option_bytes.WMSecStartSector, flash_option_bytes.WMSecEndSector);

		/* Unlock the Flash to enable the flash control register access */
		HAL_FLASH_Unlock();

		/* Unlock the Options Bytes */
		HAL_FLASH_OB_Unlock();


		PRINTF("Program option byte WM Bank1\r\n");

		flash_option_bytes.OptionType = OPTIONBYTE_WMSEC;
		flash_option_bytes.WMSecStartSector=0;
		flash_option_bytes.WMSecEndSector=0x7F;

		ret=HAL_FLASHEx_OBProgram(&flash_option_bytes);
		if (ret != HAL_OK)
		{
			printf("Error while setting WM bank1 : %d\r\n", ret);
			while(1);
		}
		obUpdate=1;
	}

	flash_option_bytes.Banks = FLASH_BANK_2;
	HAL_FLASHEx_OBGetConfig(&flash_option_bytes);


	if ((flash_option_bytes.WMSecStartSector != SECURE_WATERMARK_BANK2_START)
	 || (flash_option_bytes.WMSecEndSector != SECURE_WATERMARK_BANK2_END))
	{
		PRINTF("Flash watermarks bank2 not set correctly : Start 0x%2.2lx End 0x%2.2lx\r\n", flash_option_bytes.WMSecStartSector, flash_option_bytes.WMSecEndSector);

		/* Unlock the Flash to enable the flash control register access */
		HAL_FLASH_Unlock();

		/* Unlock the Options Bytes */
		HAL_FLASH_OB_Unlock();


		PRINTF("Program option byte WM Bank2\r\n");

		flash_option_bytes.OptionType = OPTIONBYTE_WMSEC;
		flash_option_bytes.WMSecStartSector=0x7F;
		flash_option_bytes.WMSecEndSector=0x0;

		ret=HAL_FLASHEx_OBProgram(&flash_option_bytes);
		if (ret != HAL_OK)
		{
			printf("Error while setting WM bank2 : %d\r\n", ret);
			while(1);
		}
		obUpdate=1;
	}

	if (obUpdate == 1)
	{
		PRINTF("OB Launch ...\r\n");
		ret=HAL_FLASH_OB_Launch();

		if (ret != HAL_OK)
		{
			printf("Error while execution OB_Launch\r\n");
			while(1);
		}
	}
	else
	{
		PRINTF("Secure watermarks already set\r\n");
	}
}
