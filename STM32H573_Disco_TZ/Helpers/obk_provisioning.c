#include "obk_provisioning.h"
#include "string.h" //For memcpy

// Debug authentication provisioning data
#include "DA_Config.h"

#define SHA256_LENGTH             (32U)
#define OBK_HDPL1_OFFSET          (0x100U)
#define OBK_HDPL1_END             (0x8FFU)
#define OBK_FLASH_PROG_UNIT       (0x10U)
#define ALL_OBKEYS                (0x1FFU)

#define FLASH_OBK_BASE_DA         (FLASH_OBK_BASE_S + OBK_HDPL1_OFFSET)

#define MAX_SIZE_CFG_DA           0x60
#define SBS_EXT_EPOCHSELCR_EPOCH_SEL_S_EPOCH    (1U << SBS_EPOCHSELCR_EPOCH_SEL_Pos)
#define SBS_EXT_EPOCHSELCR_EPOCH_SEL_NS_EPOCH   (0U << SBS_EPOCHSELCR_EPOCH_SEL_Pos )

typedef struct {
    uint32_t addr;
    uint32_t length;
    uint32_t encrypted;
  } OBK_Header_t;


static HASH_HandleTypeDef hhash;

static HAL_StatusTypeDef Compute_SHA256(uint8_t *pBuffer, uint32_t Length, uint8_t *pSHA256);
static int32_t OBK_Read(uint32_t Offset, void *pData, uint32_t Length);
static int32_t OBK_Flash_WriteEncrypted(uint32_t Offset, const void *pData, uint32_t Length);
static int32_t OBK_Flash_ReadEncrypted(uint32_t Offset, void *pData, uint32_t Length);

const uint32_t a_aes_iv[4] = {0x8001D1CEU, 0xD1CED1CEU, 0xD1CE8001U, 0xCED1CED1U};

/**
  * \brief      Check if the Flash memory boundaries are not violated.
  * \param[in]  flash_dev  Flash device structure \ref arm_obk_flash_dev_t
  * \param[in]  offset     Highest Flash memory address which would be accessed.
  * \return     Returns true if Flash memory boundaries are not violated, false
  *             otherwise.
  */
static uint32_t is_range_valid(uint32_t Offset)
{
  return (Offset <= OBK_HDPL1_END) ? (1) : (0);
}


/**
  * \brief  Check if the parameter is aligned to program_unit.
  * @parma  Param Any number that can be checked against the
  *               program_unit, e.g. Flash memory address or
  *               data length in bytes.
  * @retval Returns true if param is aligned to program_unit, false
  *               otherwise.
  */
static uint32_t is_write_aligned(uint32_t Param)
{
  return ((Param % 16) != 0U) ? (0) : (1);
}

/**
  * @brief  Control if the length is 16 bytes multiple (QUADWORD)
  * @param  Length: Number of bytes (multiple of 16 bytes)
  * @retval None
  */
static uint32_t is_write_allowed(uint32_t Length)
{
  return ((Length % 16) != 0U) ? (0) : (1);
}
/**
  * @brief  Compute SHA256
  * @param  pBuffer: pointer to the input buffer to be hashed
  * @param  Length: length of the input buffer in bytes
  * @param  pSHA256: pointer to the compuyed digest
  * @retval None
  */
static HAL_StatusTypeDef Compute_SHA256(uint8_t *pBuffer, uint32_t Length, uint8_t *pSHA256)
{
  /* Enable HASH clock */
  __HAL_RCC_HASH_CLK_ENABLE();

  hhash.Instance = HASH;
  /* HASH Configuration */
  if (HAL_HASH_DeInit(&hhash) != HAL_OK)
  {
    return HAL_ERROR;
  }
  hhash.Init.DataType = HASH_BYTE_SWAP;
  hhash.Init.Algorithm = HASH_ALGOSELECTION_SHA256;
  if (HAL_HASH_Init(&hhash) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* HASH computation */
  if (HAL_HASH_Start(&hhash, pBuffer, Length, pSHA256, 10) != HAL_OK)
  {
    return HAL_ERROR;
  }
  return HAL_OK;
}

/**
  * @brief  Memory compare with constant time execution.
  * @note   Objective is to avoid basic attacks based on time execution
  * @param  pAdd1 Address of the first buffer to compare
  * @param  pAdd2 Address of the second buffer to compare
  * @param  Size Size of the comparison
  * @retval SFU_ SUCCESS if equal, a SFU_error otherwise.
  */
static uint32_t MemoryCompare(uint8_t *pAdd1, uint8_t *pAdd2, uint32_t Size)
{
  uint8_t result = 0x00U;
  uint32_t i = 0U;

  for (i = 0U; i < Size; i++)
  {
    result |= pAdd1[i] ^ pAdd2[i];
  }
  return result;
}


/**
  * @brief  Write encrypted OBkeys
  * @param  Offset Offset in the OBKeys area (aligned on 16 bytes)
  * @param  pData Data buffer to be programmed encrypted (aligned on 4 bytes)
  * @param  Length Number of bytes (multiple of 16 bytes)
  * @retval error status
  */
static int32_t OBK_Flash_WriteEncrypted(uint32_t Offset, const void *pData, uint32_t Length)
{
  uint32_t i = 0U;
  uint32_t destination = FLASH_OBK_BASE_S + Offset;
  FLASH_EraseInitTypeDef FLASH_EraseInitStruct = {0U};
  uint32_t sector_error = 0U;
  CRYP_HandleTypeDef hcryp = {0U};
  uint32_t SaesTimeout = 100U;
  uint32_t DataEncrypted[MAX_SIZE_CFG_DA / 4U] = {0UL};

  /* Check parameters */
  if ((is_range_valid(Offset + Length - 1U) != 1) ||
      (is_write_aligned(Offset) != 1) ||
      (is_write_allowed(Length) != 1) ||
      (Length > MAX_SIZE_CFG_DA))
  {
    return 1;
  }

  __HAL_RCC_SBS_CLK_ENABLE();
  __HAL_RCC_SAES_CLK_ENABLE();

  /* Unlock  Flash area */
  (void) HAL_FLASH_Unlock();
  (void) HAL_FLASHEx_OBK_Unlock();

  /* Force use of EPOCH_S value for DHUK */
  WRITE_REG(SBS_S->EPOCHSELCR, SBS_EXT_EPOCHSELCR_EPOCH_SEL_S_EPOCH);

  /* Configure SAES parameters */
  hcryp.Instance = SAES_S;
  if (HAL_CRYP_DeInit(&hcryp) != HAL_OK)
  {
    return 2;
  }
  hcryp.Init.DataType = CRYP_NO_SWAP;
  hcryp.Init.KeySelect = CRYP_KEYSEL_HW;        /* Hardware key : derived hardware unique key (DHUK 256-bit) */
  hcryp.Init.Algorithm = CRYP_AES_CBC;
  hcryp.Init.KeyMode = CRYP_KEYMODE_NORMAL ;
  hcryp.Init.KeySize = CRYP_KEYSIZE_256B;       /* 256 bits AES Key */
  hcryp.Init.pInitVect = (uint32_t *)a_aes_iv;

  if (HAL_CRYP_Init(&hcryp) != HAL_OK)
  {
    return 3;
  }

  /* Size is n words */
  if (HAL_CRYP_Encrypt(&hcryp, (uint32_t *)pData, (uint16_t) (Length / 4U), &DataEncrypted[0U], SaesTimeout) != HAL_OK)
  {
    return 4;
  }
  if (HAL_CRYP_DeInit(&hcryp) != HAL_OK)
  {
    return 5;
  }

  /* Erase OBKeys */
  FLASH_EraseInitStruct.TypeErase = FLASH_TYPEERASE_OBK_ALT;
  if (HAL_FLASHEx_Erase(&FLASH_EraseInitStruct, &sector_error) != HAL_OK)
  {
    return 6;
  }

  /* Program OBKeys */
  for (i = 0U; i < Length; i += OBK_FLASH_PROG_UNIT)
  {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD_OBK_ALT, (destination + i), (uint32_t)&DataEncrypted[i / 4U]) != HAL_OK)
    {
      return 7;
    }
  }

  /* Swap all OBKeys */
  if (HAL_FLASHEx_OBK_Swap(ALL_OBKEYS) != HAL_OK)
  {
      return 8;
  }

  /* Lock the User Flash area */
  (void) HAL_FLASH_Lock();
  (void) HAL_FLASHEx_OBK_Lock();

  return 0;
}

/**
  * @brief  Read non-encrypted OBkeys
  * @param  Offset: Offset in the OBKeys area (aligned on 16 bytes)
  * @param  pData Data buffer to be filled (aligned on 4 bytes)
  * @param  Length: Number of bytes (multiple of 4 bytes)
  * @retval ARM_DRIVER error status
  */
static int32_t OBK_Read(uint32_t Offset, void *pData, uint32_t Length)
{
  uint32_t *p_source = (uint32_t *) (FLASH_OBK_BASE_S + Offset);
  uint32_t *p_destination = (uint32_t *) pData;

  /* Check parameters */
  if (is_range_valid(Offset + Length - 1U) != 1)
  {
    return 1;
  }

  for (uint32_t i=0; i<(Length/4); i++)
  {
	  *(p_destination+i) = *(p_source+i);
	  HAL_Delay(10);
  }

  return 0;
}


/**
  * @brief  Read encrypted OBkeys
  * @param  Offset Offset in the OBKeys area (aligned on 16 bytes)
  * @param  pData Data buffer to be filled (aligned on 4 bytes)
  * @param  Length Number of bytes (multiple of 4 bytes)
  * @retval ARM_DRIVER error status
  */
static int32_t OBK_Flash_ReadEncrypted(uint32_t Offset, void *pData, uint32_t Length)
{
  CRYP_HandleTypeDef hcryp = { 0U };
  uint32_t SaesTimeout = 100U;
  uint32_t DataEncrypted[MAX_SIZE_CFG_DA / 4U] = { 0UL };
  uint8_t *p_source = (uint8_t *) (FLASH_OBK_BASE_S + Offset);
  uint8_t *p_destination = (uint8_t *) DataEncrypted;
  /* Check OBKeys  boundaries */
  if (is_range_valid(Offset + Length -1U) != 1)
  {
    return 1;
  }

  /* Do not use memcpy from lib to manage properly ECC error */
  //DoubleECC_Error_Counter = 0U;
  memcpy(p_destination, p_source, Length);
//  if (DoubleECC_Error_Counter != 0U)
//  {
//    BOOT_LOG_ERR("Double ECC error detected: FLASH_ECCDETR=0x%x", (int)FLASH->ECCDETR);
//    memset(p_destination, 0x00, Length);
//  }

  __HAL_RCC_SBS_CLK_ENABLE();
  __HAL_RCC_SAES_CLK_ENABLE();

  /* Force use of EPOCH_S value for DHUK */
  WRITE_REG(SBS_S->EPOCHSELCR, SBS_EXT_EPOCHSELCR_EPOCH_SEL_S_EPOCH);

  /* Configure SAES parameters */
  hcryp.Instance = SAES_S;
  if (HAL_CRYP_DeInit(&hcryp) != HAL_OK)
  {
    return 2;
  }
  hcryp.Init.DataType  = CRYP_NO_SWAP;
  hcryp.Init.KeySelect = CRYP_KEYSEL_HW;  /* Hardware unique key (256-bits) */
  hcryp.Init.Algorithm = CRYP_AES_CBC;
  hcryp.Init.KeyMode = CRYP_KEYMODE_NORMAL ;
  hcryp.Init.KeySize = CRYP_KEYSIZE_256B; /* 256 bits AES Key*/
  hcryp.Init.pInitVect = (uint32_t *)a_aes_iv;

  if (HAL_CRYP_Init(&hcryp) != HAL_OK)
  {
    return 3;
  }

  /*Size is n words*/
  if (HAL_CRYP_Decrypt(&hcryp, (uint32_t *)&DataEncrypted[0U], (uint16_t) (Length / 4U), (uint32_t *)pData, SaesTimeout) != HAL_OK)
  {
    return 4;
  }
  if (HAL_CRYP_DeInit(&hcryp) != HAL_OK)
  {
    return 5;
  }

  return 0;
}



void OBKProvisioning_ProvisionDA(void)
{
	OBK_Header_t *pHeader;
	uint8_t *provData;
	uint8_t sha256[SHA256_LENGTH] = { 0U };

	PRINTF("Check provisioning status ...\r\n");
	if ((*(uint32_t *)(FLASH_OBK_BASE_DA)) != 0xFFFFFFFF)
	{
		PRINTF("DA Already provisioned !\r\n");
		return;
	}

	PRINTF("Provisioning DA using embedded DA config\r\n");
	pHeader = (OBK_Header_t *)DA_Config;
	provData = (uint8_t *)DA_Config + sizeof(OBK_Header_t);

	// Check consistency of DA_ConfigData buffer
	if(pHeader->encrypted != 1)
	{
		PRINTF("Wrong Header encrypted value (0x%lx)\r\n", pHeader->encrypted);
		return;
	}

	if((pHeader->addr) != 0x0FFD0100UL)
	{
		PRINTF("Wrong address (0x%lx)\r\n", pHeader->addr);
		return;
	}

	if((pHeader->length) != 0x60)
	{
		printf("Wrong size (0x%lx)\r\n", pHeader->length);
		return;
	}

	PRINTF("Check embedded DA Config Hash \r\n");
	HAL_StatusTypeDef status = Compute_SHA256((uint8_t *) (provData + SHA256_LENGTH), pHeader->length - SHA256_LENGTH, sha256);

	if (status != HAL_OK)
	{
		PRINTF("HASH fail!\r\n");
	}

	if (MemoryCompare((uint8_t *)provData, &sha256[0], SHA256_LENGTH) != 0U)
	{
		printf("Wrong hash \r\n");
		return;
	}

	PRINTF("Provisioning %2.2x %2.2x ...\r\n", provData[0], provData[1]);

	uint32_t offset = pHeader->addr - FLASH_OBK_BASE_S;
	uint32_t result = OBK_Flash_WriteEncrypted(offset, (const void*)(provData), pHeader->length);
	if (result !=0)
	{
		PRINTF("Error Writing OBK file : %ld\r\n", result);
		return;
	}

	PRINTF("Provisioning done\r\n");
	NVIC_SystemReset();
}

void OBKProvisioning_ReadDA(void)
{
	OBK_Header_t *pHeader;
	pHeader = (OBK_Header_t *)DA_Config;
	uint32_t offset = pHeader->addr - FLASH_OBK_BASE_S;
	uint8_t DABuffer[MAX_SIZE_CFG_DA];
	uint32_t result;

	printf("Read provisioned DA\r\n");
	result = OBK_Read(offset, (void *)DABuffer, pHeader->length);

	if (result != 0)
	{
		printf("Error OBK_Read\r\n");
	}

	for (uint32_t i=0; i<(MAX_SIZE_CFG_DA/8); i++)
	{
		for (uint32_t j=0; j<8; j++)
		{
			printf("0x%2.2x ", DABuffer[i*8+j]);
		}
		printf("\r\n");
	}

	printf("\r\nDecrypt provisioned DA\r\n");
	result = OBK_Flash_ReadEncrypted(offset, (void *)DABuffer, pHeader->length);

	if (result != 0)
	{
		printf("Error OBK_Flash_ReadEncrypted\r\n");
	}

	for (uint32_t i=0; i<(MAX_SIZE_CFG_DA/8); i++)
	{
		for (uint32_t j=0; j<8; j++)
		{
			printf("0x%2.2x ", DABuffer[i*8+j]);
		}
		printf("\r\n");
	}
}








