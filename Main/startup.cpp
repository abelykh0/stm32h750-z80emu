#include <camera/CameraScreen.h>
#include "stm32h7xx_hal.h"
#include <stdio.h>

#include "gpio.h"
#include "tim.h"
//#include "usb_host.h"
//#include "usbh_hid.h"
#include "fatfs.h"

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_video_if.h"

#include "w25qxx_qspi.h"
#include "vga.h"
#include "resources.h"
#include "config.h"
#include "camera/CameraScreen.h"
#include "screen.h"
#include "emulator.h"
#include "sdcard.h"
#include "emulator/videoRam.h"
#include "emulator/z80main.h"
#include "emulator/z80input.h"
#include "keyboard/ps2Keyboard.h"

#include "demo_colors/gradient.h"
#include "demo_colors/display_bmp.h"

Camera::CameraScreen cameraScreen;
Display::Screen fullScreen;

//extern USBH_HandleTypeDef hUsbHostHS;

static void MapFlash();

extern "C" void initialize()
{
	PrepareClut();
	HAL_PWREx_EnableUSBVoltageDetector();
}

extern "C" void setup()
{
	MapFlash();

/*
	InitCamera();
	cameraScreen.SetAttribute(0x2A10);
	cameraScreen.Clear();
*/

/*
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
*/
	//gradient(VideoRam, L8Clut);


#ifdef STATIC_IMAGE
	// Display static BMP from QSPI
	memcpy(L8Clut, bmp1920x1080 + 54, sizeof(L8Clut));
#endif

	LtdcInit();

#ifndef STATIC_IMAGE
	fullScreen.Clear();

	Ps2_Initialize();
	zx_setup();
#endif
}

extern "C" void loop()
{
#ifdef STATIC_IMAGE
	HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
	HAL_Delay(500);
	return;
#endif

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

	int32_t result = zx_loop();
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

