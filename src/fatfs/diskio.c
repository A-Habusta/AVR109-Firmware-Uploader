/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

/***
 * Modified 03.02.2024 by Adrián Habušta
 * 	- Added implementation for skeleton methods, which are required by FatFs
 * Modified 12.02.2024 by Adrián Habušta
 * 	- Finish implementation
 ***/

#include <stdbool.h>

#include "fatfs/ff.h"			/* Obtains integer types */
#include "fatfs/diskio.h"		/* Declarations of disk functions */
#include "sd.h"
#include "clcd.h"

static inline DRESULT sd_error_to_result(sd_error_t err) {
	switch (err) {
		case SD_ERROR_OK:
			return RES_OK;
		case SD_ERROR_IDLE:
			return RES_NOTRDY;
		default:
			return RES_ERROR;
	}
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	(void) pdrv;

	bool initialized = sd_is_initialized();
	return initialized ? STA_OK : STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	(void) pdrv;

	sd_error_t err = sd_init();

	if (err == SD_ERROR_TIMEOUT) {
		return STA_NODISK;
	} else if (err != SD_ERROR_OK) {
		return STA_NOINIT;
	}

	return STA_OK;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

/* Will not be called with count > 1 in current configuration */
DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	(void) pdrv;

	if (count != 1) {
		return RES_PARERR;
	}

	sd_error_t err = sd_read_block(buff, sector);
	return sd_error_to_result(err);
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

/* Not used in current configuration */
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	(void) pdrv;
	(void) buff;
	(void) sector;
	(void) count;

	return RES_ERROR;
}

/*-----------------------------------------------------------------------*/
/* Misc Functions                                                        */
/*-----------------------------------------------------------------------*/

/* Not used in current configuration */
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	(void) pdrv;
	(void) cmd;
	(void) buff;

	return RES_ERROR;
}