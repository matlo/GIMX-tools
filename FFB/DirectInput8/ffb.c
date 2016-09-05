/*
 * DirectInput8 Force Feedback test application:
 * - picks the first DI joystick with FF support
 * - prints its name
 * - enumerates its axes supporting FF
 * - prints all supported effects types and parameters
 * - if supported, plays spring and damper effects
 *
 * Compile with: gcc -Wall -o ffb ffb.c -ldinput8 -ldxguid -lole32 -static
 *
 * Copyright (c) 2016 Mathieu Laurendeau <mat.lau@laposte.net>
 * License: GPLv3
 */

#include <stdio.h>
#include <dinput.h>

static LPDIRECTINPUT8 dinput = NULL;
static LPDIRECTINPUTDEVICE8 device = NULL;
static HWND raw_hwnd = NULL;
static ATOM class_atom = 0;
static DWORD axis = -1;

static unsigned char hasSpring = 0;
static unsigned char hasDamper = 0;

static int DI_GUIDIsSame(const GUID * a, const GUID * b)
{
    return (memcmp(a, b, sizeof (GUID)) == 0);
}

static BOOL CALLBACK DI_DeviceObjectCallback(LPCDIDEVICEOBJECTINSTANCE dev, LPVOID pvRef)
{
	const GUID *guid = &dev->guidType;
	DWORD offset = -1;
	const char * name = NULL;
	if (DI_GUIDIsSame(guid, &GUID_XAxis)) {
		offset = DIJOFS_X;
		name = "X";
	} else if (DI_GUIDIsSame(guid, &GUID_YAxis)) {
		offset = DIJOFS_Y;
		name = "Y";
	} else if (DI_GUIDIsSame(guid, &GUID_ZAxis)) {
		offset = DIJOFS_Z;
		name = "Z";
	} else if (DI_GUIDIsSame(guid, &GUID_RxAxis)) {
		offset = DIJOFS_RX;
		name = "RX";
	} else if (DI_GUIDIsSame(guid, &GUID_RyAxis)) {
		offset = DIJOFS_RY;
		name = "RY";
	} else if (DI_GUIDIsSame(guid, &GUID_RzAxis)) {
		offset = DIJOFS_RZ;
		name = "RZ";
	}
	
	if (offset != -1) {
		printf("Found axis with haptic capabilities: %s\n", name);fflush(stdout);
		if (axis == -1) {
			axis = offset;
		}
	}

    return DIENUM_CONTINUE;
}

const struct {
	const GUID * guid;
	const char * name;
} effects[] = {
	{ &GUID_Spring, "spring" },
    { &GUID_Damper, "damper" },
    { &GUID_Inertia, "inertia" },
    { &GUID_Friction, "friction" },
    { &GUID_ConstantForce, "constant force" },
    { &GUID_CustomForce, "custom force" },
    { &GUID_Sine, "sine" },
    { &GUID_Square, "square" },
    { &GUID_Triangle, "triangle" },
    { &GUID_SawtoothUp, "sawtoothup" },
    { &GUID_SawtoothDown, "sawtoothdown" },
    { &GUID_RampForce, "ramp force" }
};

const struct {
	DWORD dwEffType;
	const char * name;
} effectType[] = {
	{ DIEFT_CONDITION,          "condition" },
    { DIEFT_CONSTANTFORCE,      "constant force" },
    { DIEFT_CUSTOMFORCE,        "custom force" },
    { DIEFT_DEADBAND,           "deadband" },
    { DIEFT_FFATTACK,           "attack" },
    { DIEFT_FFFADE,             "fade" },
    { DIEFT_HARDWARE,           "hardware" },
    { DIEFT_PERIODIC,           "periodic" },
    { DIEFT_POSNEGCOEFFICIENTS, "pos and neg coefficients" },
    { DIEFT_POSNEGSATURATION,   "pos and neg saturation" },
    { DIEFT_RAMPFORCE,          "ramp force" },
    { DIEFT_SATURATION,         "saturation" },
    { DIEFT_STARTDELAY,         "start delay" },
};

const struct {
	DWORD dwParam;
	const char * name;
} effectParam[] = {
    { DIEP_ALLPARAMS,    "all params" },
	{ DIEP_AXES,         "axes" },
	{ DIEP_DIRECTION,    "direction" },
	{ DIEP_DURATION,     "duration" },
	{ DIEP_ENVELOPE,     "enveloppe" },
	{ DIEP_GAIN,         "gain" },
	{ DIEP_SAMPLEPERIOD, "sample period" },
	{ DIEP_STARTDELAY,   "start delay" },
	{ DIEP_TRIGGERBUTTON, "trigger button" },
	{ DIEP_TRIGGERREPEATINTERVAL, "trigger repeat interval" },
	{ DIEP_TYPESPECIFICPARAMS, "type specific params" },
};

static BOOL CALLBACK
DI_EffectCallback(LPCDIEFFECTINFO pei, LPVOID pv)
{	
	if (DI_GUIDIsSame(&pei->guid, &GUID_Spring)) {
		hasSpring = 1;
	} else if (DI_GUIDIsSame(&pei->guid, &GUID_Damper)) {
		hasDamper = 1;
	}
	unsigned int i;
	for (i = 0; i < sizeof(effects) / sizeof(*effects); ++i) {
		if (DI_GUIDIsSame(&pei->guid, effects[i].guid)) {
			printf("  %s\n", effects[i].name);
			unsigned int j;
			printf("    types: ");
			for (j = 0; j < sizeof(effectType) / sizeof(*effectType); ++j) {
				if (pei->dwEffType & effectType[j].dwEffType) {
					printf("%s, ", effectType[j].name);
				}
			}
			printf("\n");
			printf("    params:\n");
			printf("      static: ");
			for (j = 0; j < sizeof(effectParam) / sizeof(*effectParam); ++j) {
				if (pei->dwStaticParams & effectParam[j].dwParam) {
					printf("%s, ", effectParam[j].name);
				}
			}
			printf("\n");
			printf("      dynamic: ");
			for (j = 0; j < sizeof(effectParam) / sizeof(*effectParam); ++j) {
				if (pei->dwDynamicParams & effectParam[j].dwParam) {
					printf("%s, ", effectParam[j].name);
				}
			}
			printf("\n");
			break;
		}
	}

    return DIENUM_CONTINUE;
}

static LRESULT CALLBACK RawWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {

    if (Msg == WM_DESTROY) {
        return 0;
    }

    return DefWindowProc(hWnd, Msg, wParam, lParam);
}

static HWND createWindow() {
	
	HANDLE hInstance = GetModuleHandle(NULL);
    
    WNDCLASSEX wce = {
      .cbSize = sizeof(WNDCLASSEX),
      .lpfnWndProc = RawWndProc,
      .lpszClassName = "DI test app class",
      .hInstance = hInstance,
    };
    class_atom = RegisterClassEx(&wce);
    if (class_atom == 0)
        return NULL;
    
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);
    
    return CreateWindow(wce.lpszClassName, "DI test app", WS_POPUP | WS_VISIBLE | WS_SYSMENU, cursor_pos.x, cursor_pos.y, 1, 1, NULL, NULL, hInstance, NULL);
}

static void applyCondition(const char * msg, const GUID * guid, LONG lCoeff, LONG rCoeff, DWORD lSat, DWORD rSat) {
	
	DWORD rgdwAxes[1] = { axis };
	LONG rglDirection [sizeof(rgdwAxes) / sizeof(*rgdwAxes)] = { 9000 };
	
	DICONDITION conditions[1] = {
		{
			.lOffset = 0,
			.lPositiveCoefficient = rCoeff,
			.lNegativeCoefficient = lCoeff,
			.dwPositiveSaturation = rSat,
			.dwNegativeSaturation = lSat,
			.lDeadBand = 0
		}
	};
	
	DIEFFECT effect = { 
		.dwSize = sizeof(DIEFFECT),
		.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_CARTESIAN,
		.dwDuration = INFINITE,
		.dwSamplePeriod = 0,
		.dwGain = DI_FFNOMINALMAX,
		.dwTriggerButton = DIEB_NOTRIGGER,
		.dwTriggerRepeatInterval = INFINITE,
		.cAxes = sizeof(rgdwAxes) / sizeof(*rgdwAxes),
		.rgdwAxes = rgdwAxes,
		.lpEnvelope = NULL,
		.rglDirection  = rglDirection ,
        .cbTypeSpecificParams = sizeof(conditions),
        .lpvTypeSpecificParams = conditions,
		.dwStartDelay = 0
	};
	
	LPDIRECTINPUTEFFECT pdeff;
	
	HRESULT ret = IDirectInputDevice8_CreateEffect(device, guid, &effect, &pdeff, NULL);
    if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_CreateEffect failed\n");
        return;
    }
	
    ret = IDirectInputEffect_Start(pdeff, INFINITE, 0);
    if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputEffect_Start failed\n");
        return;
    }
	
	printf(msg);fflush(stdout);
	
	Sleep(10000);

    ret = IDirectInputEffect_Stop(pdeff);
    if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputEffect_Stop failed\n");
    }
	
	ret = IDirectInputEffect_Unload(pdeff);
    if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputEffect_Stop failed\n");
    }
}

static BOOL CALLBACK EnumHapticsCallback(const DIDEVICEINSTANCE * pdidInstance, VOID * pContext)
{
    (void) pContext;
	
	printf("Found haptic device: %s\n", pdidInstance->tszProductName);fflush(stdout); // in real apps this should probably be converted
	
	HRESULT ret = IDirectInput8_CreateDevice(dinput, &pdidInstance->guidInstance, &device, NULL);
    if (FAILED(ret)) {
        fprintf(stderr, "IDirectInput8_CreateDevice failed\n");
        return DIENUM_CONTINUE;
    }
	
    return DIENUM_STOP;
}

int initHapticDevice()
{
	HRESULT ret = IDirectInputDevice8_SetCooperativeLevel(device, raw_hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInput8_CreateDevice failed\n");
        return -1;
	}
	
	//IDirectInputDevice8_RunControlPanel(device, raw_hwnd, 0);
	
	ret = IDirectInputDevice8_SetDataFormat(device, &c_dfDIJoystick2);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInput8_CreateDevice failed\n");
        return -1;
	}
	
	ret = IDirectInputDevice8_Acquire(device);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_Acquire failed\n");
        return -1;
	}

	ret = IDirectInputDevice8_SendForceFeedbackCommand(device, DISFFC_RESET);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_SendForceFeedbackCommand failed\n");
        return -1;
	}

    ret = IDirectInputDevice8_SendForceFeedbackCommand(device, DISFFC_SETACTUATORSON);
    if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_SendForceFeedbackCommand failed\n");
        return -1;
	}
	
	ret = IDirectInputDevice8_Unacquire(device);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_Unacquire failed\n");
	}
	
    DIPROPDWORD dipdw = {
		.diph = {
			.dwSize = sizeof( DIPROPDWORD ),
			.dwHeaderSize = sizeof( DIPROPHEADER ),
			.dwObj = 0,
			.dwHow = DIPH_DEVICE
		},
		.dwData = DIPROPAUTOCENTER_OFF
	};
    ret = IDirectInputDevice8_SetProperty(device, DIPROP_AUTOCENTER, &dipdw.diph);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_SetProperty failed\n");
	}
	
	ret = IDirectInputDevice8_EnumObjects(device, DI_DeviceObjectCallback, NULL, DIDFT_AXIS | DIDFT_FFACTUATOR);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_EnumObjects failed\n");
        return -1;
	}
	
	printf("Supported effects: \n");
	
    ret = IDirectInputDevice8_EnumEffects(device, DI_EffectCallback, NULL, DIEFT_ALL);
    if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_EnumEffects failed\n");
        return -1;
    }
	
	fflush(stdout);
	
	ret = IDirectInputDevice8_Acquire(device);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_Acquire failed\n");
        return -1;
	}
	
	return 0;
}

int closeHapticDevice() {

	HRESULT ret = IDirectInputDevice8_SendForceFeedbackCommand(device, DISFFC_RESET);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_SendForceFeedbackCommand failed\n");
        return -1;
	}

	ret = IDirectInputDevice8_Unacquire(device);
	if (FAILED(ret)) {
        fprintf(stderr, "IDirectInputDevice8_Unacquire failed\n");
	}
    
	return 0;
}

int main(int argc, char * argv[]) {

	raw_hwnd = createWindow();
	if (raw_hwnd == NULL) {
		DWORD error = GetLastError();
		LPTSTR pBuffer = NULL;
		if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, (LPTSTR) & pBuffer, 0, NULL))
		{
			fprintf(stderr, "code = %lu\n", error);
		}
		else
		{
			fprintf(stderr, "%s\n", pBuffer);
			LocalFree(pBuffer);
		}
        fprintf(stderr, "createWindow failed\n");
		return -1;
	}
    
    ShowWindow(raw_hwnd, SW_SHOW);

	HRESULT ret = CoInitializeEx(NULL, 0);
    if (FAILED(ret)) {
        fprintf(stderr, "Coinitialize failed\n");
		return -1;
    }

    ret = CoCreateInstance(&CLSID_DirectInput8, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectInput8, (LPVOID)& dinput);
    if (FAILED(ret)) {
        fprintf(stderr, "CoCreateInstance failed\n");
		return -1;
    }

    HINSTANCE instance = GetModuleHandle(NULL);
    if (instance == NULL) {
		fprintf(stderr, "GetModuleHandle failed with error code %lu\n", GetLastError());
		return -1;
    }

    ret = IDirectInput8_Initialize(dinput, instance, DIRECTINPUT_VERSION);
    if (FAILED(ret)) {
		fprintf(stderr, "IDirectInput8_Initialize failed\n");
		return -1;
    }

    ret = IDirectInput8_EnumDevices(dinput, 0, EnumHapticsCallback, NULL, DIEDFL_FORCEFEEDBACK | DIEDFL_ATTACHEDONLY);
    if (FAILED(ret)) {
		fprintf(stderr, "IDirectInput8_EnumDevices failed\n");
		return -1;
    }
	
	if (device == NULL) {
		fprintf(stderr, "No haptic device found\n");
		return -1;
	}
	
	if (initHapticDevice() < 0) {
		return -1;
	}
	
	if (hasSpring) {
		applyCondition("Playing left spring effect\n", &GUID_Spring, DI_FFNOMINALMAX, 0, DI_FFNOMINALMAX, 0);
		applyCondition("Playing right spring effect\n", &GUID_Spring, 0, DI_FFNOMINALMAX, 0, DI_FFNOMINALMAX);
	}

	if (hasDamper) {
		applyCondition("Playing left damper effect\n", &GUID_Damper, DI_FFNOMINALMAX, 0, DI_FFNOMINALMAX, 0);
		applyCondition("Playing right damper effect\n", &GUID_Damper, 0, DI_FFNOMINALMAX, 0, DI_FFNOMINALMAX);
	}
	
	closeHapticDevice();
	
	return 0;
}
