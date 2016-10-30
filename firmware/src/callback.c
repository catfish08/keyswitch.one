/*******************************************************************************
 * @file callback.c
 * @brief USB Callbacks.
 *******************************************************************************/
//=============================================================================
// src/callback.c: generated by Hardware Configurator
//
// This file is only generated if it does not exist. Modifications in this file
// will persist even if Configurator generates code. To refresh this file,
// you must first delete it and then regenerate code.
//=============================================================================
//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <SI_EFM8UB1_Register_Enums.h>
#include <efm8_usb.h>
#include "descriptors.h"
#include "idle.h"
// Empty report
SI_SEGMENT_VARIABLE(noKeyReport, const KeyReport_TypeDef, SI_SEG_CODE) = { 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
// Desired Report (H)
SI_SEGMENT_VARIABLE(keyReport, const KeyReport_TypeDef, SI_SEG_XDATA) = { 0x02,
		0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00 };



SI_SBIT(LED0, SFR_P0, 7);
SI_SBIT(SW0, SFR_P0, 3);

uint8_t tmpBuffer;
uint8_t capsLock;
uint8_t button_history;

#define MASK   0xC7

void update_button(uint8_t *button_history, uint8_t button_value) {
	*button_history = *button_history << 1;
	*button_history |= button_value;
}

bool is_button_pressed(uint8_t *button_history) {
	if ((*button_history & MASK) == 0x07) {
		*button_history = 0xFF;
		return true;
	} else {
		return false;
	}
}

bool is_button_released(uint8_t *button_history) {
	uint8_t released = 0;
	if ((*button_history & MASK) == 0xC0) {
		*button_history = 0x00;
		return true;
	} else {
		return false;
	}
}

uint16_t USBD_XferCompleteCb(uint8_t epAddr, USB_Status_TypeDef status,
		uint16_t xferred, uint16_t remaining) {
	UNREFERENCED_ARGUMENT(xferred);
	UNREFERENCED_ARGUMENT(remaining);

	if (status == USB_STATUS_OK) {
		// The only output reported supported is the SetReport to enable
		// Num Key and Caps Lock LED's.
		if (epAddr == EP0) {
			capsLock = !((bool) (tmpBuffer & 0x02));
		}
	}

	return 0;

}

uint8_t press_count = 0;
uint8_t keyPushed = 0;
uint8_t lastAcknowledgedState = 0;

#if SLAB_USB_SOF_CB
void USBD_SofCb(uint16_t sofNr) {

	int8_t status;
	bool isKeyDown = (SW0 == 0);
	bool isKeyEvent = false;

	UNREFERENCED_ARGUMENT(sofNr);

	// report to host w/ idle tick count period interval
	idleTimerTick();

	update_button(&button_history, isKeyDown);

	if (is_button_pressed(&button_history)) {
		keyPushed = true;
		isKeyEvent = true;
	}
	if (is_button_released(&button_history)) {
		keyPushed = false;
		isKeyEvent = true;
	}

	if (isIdleTimerExpired() || isKeyEvent
			|| lastAcknowledgedState != keyPushed) {
		if (keyPushed) {
			status = USBD_Write(EP1IN, &keyReport, sizeof(KeyReport_TypeDef),
					false);
		} else {
			status = USBD_Write(EP1IN, &noKeyReport, sizeof(KeyReport_TypeDef),
					false);
		}
	}

	// make sure the message goes through
	if (status == USB_STATUS_OK) {
		lastAcknowledgedState = keyPushed;
	}

}
#endif

#if SLAB_USB_SETUP_CMD_CB
USB_Status_TypeDef USBD_SetupCmdCb(
		SI_VARIABLE_SEGMENT_POINTER(setup, USB_Setup_TypeDef, MEM_MODEL_SEG)) {
	USB_Status_TypeDef retVal = USB_STATUS_REQ_UNHANDLED;

	if ((setup->bmRequestType.Type == USB_SETUP_TYPE_STANDARD)
			&& (setup->bmRequestType.Direction == USB_SETUP_DIR_IN)
			&& (setup->bmRequestType.Recipient == USB_SETUP_RECIPIENT_INTERFACE)) {
		// A HID device must extend the standard GET_DESCRIPTOR command
		// with support for HID descriptors.

		switch (setup->bRequest) {
		case GET_DESCRIPTOR:
			if ((setup->wValue >> 8) == USB_HID_REPORT_DESCRIPTOR) {
				switch (setup->wIndex) {
				case 0: // Interface 0
					USBD_Write(EP0, ReportDescriptor0,
							EFM8_MIN(sizeof(ReportDescriptor0), setup->wLength),
							false);
					retVal = USB_STATUS_OK;
					break;

				default: // Unhandled Interface
					break;
				}
			} else if ((setup->wValue >> 8) == USB_HID_DESCRIPTOR) {
				switch (setup->wIndex) {
				case 0: // Interface 0
					USBD_Write(EP0, (&configDesc[18]),
							EFM8_MIN(USB_HID_DESCSIZE, setup->wLength), false);
					retVal = USB_STATUS_OK;
					break;

				default: // Unhandled Interface
					break;
				}
			}
			break;
		}
	} else if ((setup->bmRequestType.Type == USB_SETUP_TYPE_CLASS)
			&& (setup->bmRequestType.Recipient == USB_SETUP_RECIPIENT_INTERFACE)
			&& (setup->wIndex == HID_KEYBOARD_IFC)) {
		switch (setup->bRequest) {
		case USB_HID_SET_REPORT:
			if (((setup->wValue >> 8) == 2)             // Output report
			&& ((setup->wValue & 0xFF) == 0)          	// Report ID
					&& (setup->wLength == 1)             // Report length
					&& (setup->bmRequestType.Direction != USB_SETUP_DIR_IN)) {

				USBD_Read(EP0, &tmpBuffer, 1, true);
				retVal = USB_STATUS_OK;
			} else {
				USBD_Read(EP0, &keyReport, 8, false);
				retVal = USB_STATUS_OK;
			}
			break;
		case USB_HID_GET_REPORT:
			if (((setup->wValue >> 8) == 1)           // Input report
			&& ((setup->wValue & 0xFF) == 0)          // Report ID
					&& (setup->wLength == 8)          // Report length
					&& (setup->bmRequestType.Direction == USB_SETUP_DIR_IN)) {

				if (keyPushed)
					USBD_Write(EP1IN, &noKeyReport, sizeof(KeyReport_TypeDef),
							false);
				else
					USBD_Write(EP1IN, &keyReport, sizeof(KeyReport_TypeDef),
							false);
			} else {
				USBD_Write(EP0, &keyReport, 8, false);
			}
			retVal = USB_STATUS_OK;
			break;

		case USB_HID_SET_IDLE:
			if (((setup->wValue & 0xFF) == 0)             // Report ID
			&& (setup->wLength == 0)
					&& (setup->bmRequestType.Direction != USB_SETUP_DIR_IN)) {
				idleTimerSet(setup->wValue >> 8);
				retVal = USB_STATUS_OK;
			}
			break;

		case USB_HID_GET_IDLE:
			if ((setup->wValue == 0)                      // Report ID
			&& (setup->wLength == 1)
					&& (setup->bmRequestType.Direction == USB_SETUP_DIR_IN)) {
				tmpBuffer = idleGetRate();
				USBD_Write(EP0, &tmpBuffer, 1, false);
				retVal = USB_STATUS_OK;
			}
			break;
		}
	}
	return retVal;
}
#endif

#if SLAB_USB_REMOTE_WAKEUP_ENABLED
bool USBD_RemoteWakeupCb(void) {
	// Return true if a remote wakeup event was the cause of the device
	// exiting suspend mode.
	// Otherwise return false
	return false;
}

void USBD_RemoteWakeupDelay(void) {
	// Delay 10 - 15 ms here
}
#endif

