#include "stm32h7xx_hal.h"
#include <stdio.h>

#include "gpio.h"
#include "tim.h"
#include "usb_host.h"
#include "usbh_hid.h"
#include "fatfs.h"

#include "w25qxx_qspi.h"
#include "vga.h"
#include "resources.h"
#include "config.h"
#include "emulator.h"
#include "sdcard.h"
#include "emulator/videoRam.h"
#include "emulator/z80main.h"
#include "emulator/z80input.h"
#include "keyboard/keyboard.h"

#include "demo_colors/gradient.h"
#include "demo_colors/display_bmp.h"

Display::Screen fullScreen;
extern USBH_HandleTypeDef hUsbHostFS;

static void MapFlash();

extern "C" void initialize()
{
	PrepareClut();
	HAL_PWREx_EnableUSBVoltageDetector();
}

extern "C" void setup()
{
	MapFlash();

	if (f_mount(&SDFatFS, SDPath, 1) == FR_OK)
	{
		FIL file;
		if (f_open(&file, (TCHAR*)u8"/Squirrel720x400.bmp", FA_READ) == FR_OK)
		{
			load_bmp_image(&file, VideoRam, L8Clut);

			f_close(&file);
		}

		f_mount(nullptr, nullptr, 1);
	}

	//gradient(VideoRam, L8Clut);

	LtdcInit();

	//fullScreen.Clear();
	HAL_Delay(100);
	USBH_LL_Connect(&hUsbHostFS);
	NVIC_SetPendingIRQ(OTG_FS_IRQn);
	HAL_TIM_Base_Start_IT(&htim7);



	//videoRam.ShowScreenshot(spectrumKeyboard);
	//zx_setup();
}

extern "C" void loop()
{
	//MX_USB_HOST_Process();
	return;
	if (loadSnapshotLoop())
	{
		return;
	}

	if (saveSnapshotLoop())
	{
		return;
	}

	if (showKeyboardLoop())
	{
		return;
	}

	zx_loop();
	int8_t result = GetScanCode(false);
	switch (result)
	{
	case KEY_ESCAPE:
		clearHelp();
		break;

	case KEY_F1:
		toggleHelp();
		break;

	case KEY_F2:
		if (!saveSnapshotSetup())
		{
			showErrorMessage("Cannot initialize SD card");
		}
		break;

	case KEY_F3:
		if (!loadSnapshotSetup())
		{
			showErrorMessage("Error when loading from SD card");
		}
		break;

	case KEY_F5:
		zx_reset();
		showHelp();
		break;

	case KEY_F10:
		showKeyboardSetup();
		break;

	case KEY_F12:
		showRegisters();
		break;
	}
}

extern "C" uint32_t HAL_GetTick(void)
{
  return uwTick;
}

extern "C" bool onHardFault()
{
	/*
	uint32_t cfsr = SCB->CFSR; // Configurable Fault Status Register
	uint32_t hfsr = SCB->HFSR; // Hard Fault Status Register
	uint32_t mmfar = SCB->MMFAR; // Memory Management Fault Address
	uint32_t bfar = SCB->BFAR; // Bus Fault Address
	char buffer[20];
	sprintf(buffer, "%08lX", hfsr);
	*/
	return true;
}

static void MapFlash()
{
	w25qxx_Init();
	w25qxx_EnterQPI();
	w25qxx_Startup(w25qxx_NormalMode); // w25qxx_DTRMode
}
