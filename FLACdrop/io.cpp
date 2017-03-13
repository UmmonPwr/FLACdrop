#include "stdafx.h"
#include "helper_encode.h"

extern sEncoderSettings EncSettings;

//
//  FUNCTION: RegOut()
//
//  PURPOSE: Writes the settings to the registry
//
int RegOut()
{
	HKEY hKey;
	DWORD err, temp;

	// create registry key
	err = RegCreateKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\FLACdrop", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY, 0, &hKey, NULL);
	if(err != ERROR_SUCCESS) return FAIL_REGISTRY_OPEN;
	
	// write encoder settings
	err = RegSetValueEx(hKey, L"FLAC_Quality", 0, REG_DWORD, (LPBYTE) &EncSettings.FLAC_EncodingQuality, sizeof(DWORD));
	if(err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	if(EncSettings.FLAC_Verify == true) temp = 1;
	else temp = 0;
	err = RegSetValueEx(hKey, L"FLAC_Verify",0, REG_DWORD, (LPBYTE)&temp, sizeof(DWORD));
	if(err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	if(EncSettings.FLAC_MD5check == true) temp = 1;
	else temp = 0;
	err = RegSetValueEx(hKey, L"FLAC_MD5check",0, REG_DWORD, (LPBYTE)&temp, sizeof(DWORD));
	if(err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"MP3_Int_Quality", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_InternalEncodingQuality, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"MP3_CBR_Bitrate", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_CBRBitrate, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"MP3_VBR_Quality", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_VBRQuality, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"MP3_Enc_Type", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_EncodingMode, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"OUT_Type", 0, REG_DWORD, (LPBYTE)&EncSettings.OUT_Type, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"OUT_Threads", 0, REG_DWORD, (LPBYTE)&EncSettings.OUT_Threads, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	RegCloseKey(hKey);
	return 0;
}

//
//  FUNCTION:	RegIn()
//
//  PURPOSE:	Reads the settings from the registry
//
int RegIn()
{
	HKEY hKey;
	DWORD cb, type, err, temp;

	// check if registry manipulating is working
	err = RegCreateKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\FLACdrop", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_SET_VALUE | KEY_CREATE_SUB_KEY, NULL, &hKey, NULL);
	if(err != ERROR_SUCCESS) return FAIL_REGISTRY_OPEN;

	// load FLAC quality setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"FLAC_Quality", 0, &type, (LPBYTE)&EncSettings.FLAC_EncodingQuality, &cb);
	if((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.FLAC_EncodingQuality = FLAC_ENCODINGQUALITY;
		RegDeleteValue(hKey, L"FLAC_Quality");
		RegSetValueEx(hKey, L"FLAC_Quality", 0, REG_DWORD, (LPBYTE) &EncSettings.FLAC_EncodingQuality, sizeof(DWORD));
	}
	else if (EncSettings.FLAC_EncodingQuality > FLAC_MAXENCODINGQUALITY) EncSettings.FLAC_EncodingQuality = FLAC_MAXENCODINGQUALITY; // check the loaded value if it is below the max setting
	
	// load FLAC verify setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"FLAC_Verify", 0, &type, (LPBYTE)&temp, &cb);
	if((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.FLAC_Verify = FLAC_VERIFY;
		if (EncSettings.FLAC_Verify == true) temp = 1;
		else temp = 0;
		RegDeleteValue(hKey, L"FLAC_Verify");
		RegSetValueEx(hKey, L"FLAC_Verify", 0, REG_DWORD, (LPBYTE) &temp, sizeof(DWORD));
	}
	if (temp == 0) EncSettings.FLAC_Verify = false;
	else EncSettings.FLAC_Verify = true;

	// load FLAC MD5 check setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"FLAC_MD5check", 0, &type, (LPBYTE)&temp, &cb);
	if((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.FLAC_MD5check = FLAC_MD5CHECK;
		if (EncSettings.FLAC_MD5check == true) temp = 1;
		else temp = 0;
		RegDeleteValue(hKey, L"FLAC_MD5check");
		RegSetValueEx(hKey, L"FLAC_MD5check", 0, REG_DWORD, (LPBYTE) &temp, sizeof(DWORD));
	}
	if (temp == 0) EncSettings.FLAC_MD5check = false;
	else EncSettings.FLAC_MD5check = true;

	// load MP3 internal encoder quality setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"MP3_Int_Quality", 0, &type, (LPBYTE)&EncSettings.LAME_InternalEncodingQuality, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.LAME_InternalEncodingQuality = LAME_INTERNALQUALITY;
		RegDeleteValue(hKey, L"MP3_Int_Quality");
		RegSetValueEx(hKey, L"MP3_Int_Quality", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_InternalEncodingQuality, sizeof(DWORD));
	}
	else if (EncSettings.LAME_InternalEncodingQuality > LAME_MAXINTERNALQUALITY) EncSettings.LAME_InternalEncodingQuality = LAME_MAXINTERNALQUALITY; // check the loaded value if it is below the max setting
	
	// load MP3 encoding type (CBR or VBR)
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"MP3_Enc_Type", 0, &type, (LPBYTE)&EncSettings.LAME_EncodingMode, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.LAME_EncodingMode = LAME_ENCTYPE;
		RegDeleteValue(hKey, L"MP3_Enc_Type");
		RegSetValueEx(hKey, L"MP3_Enc_Type", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_EncodingMode, sizeof(DWORD));
	}
	else if (EncSettings.LAME_EncodingMode<0 && LAME_ENCTYPE>1) EncSettings.LAME_EncodingMode = LAME_ENCTYPE;

	// load MP3 encoder CBR bitrate setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"MP3_CBR_Bitrate", 0, &type, (LPBYTE)&EncSettings.LAME_CBRBitrate, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.LAME_CBRBitrate = LAME_CBRBITRATE;
		RegDeleteValue(hKey, L"MP3_CBR_Bitrate");
		RegSetValueEx(hKey, L"MP3_CBR_Bitrate", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_CBRBitrate, sizeof(DWORD));
	}
	else if (EncSettings.LAME_CBRBitrate > LAME_CBRBITRATES_QUANTITY) EncSettings.LAME_CBRBitrate = LAME_CBRBITRATES_QUANTITY; // check the loaded value if it is below the max setting

	// load MP3 encoder VBR quality setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"MP3_VBR_Quality", 0, &type, (LPBYTE)&EncSettings.LAME_VBRQuality, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.LAME_VBRQuality = LAME_VBRQUALITY;
		RegDeleteValue(hKey, L"MP3_VBR_Quality");
		RegSetValueEx(hKey, L"MP3_VBR_Quality", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_VBRQuality, sizeof(DWORD));
	}
	else if (EncSettings.LAME_VBRQuality> LAME_MAXVBRQUALITY) EncSettings.LAME_VBRQuality = LAME_MAXVBRQUALITY; // check the loaded value if it is below the max setting

	// load output type setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"OUT_Type", 0, &type, (LPBYTE)&EncSettings.OUT_Type, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.OUT_Type = OUT_TYPE;
		RegDeleteValue(hKey, L"OUT_Type");
		RegSetValueEx(hKey, L"OUT_Type", 0, REG_DWORD, (LPBYTE)&EncSettings.OUT_Type, sizeof(DWORD));
	}
	else if (EncSettings.OUT_Type < 0 && EncSettings.OUT_Type > 1) EncSettings.OUT_Type = OUT_TYPE; // check the loaded value

	// load thread number setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"OUT_Threads", 0, &type, (LPBYTE)&EncSettings.OUT_Threads, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.OUT_Threads = OUT_THREADS;
		RegDeleteValue(hKey, L"OUT_Threads");
		RegSetValueEx(hKey, L"OUT_Threads", 0, REG_DWORD, (LPBYTE)&EncSettings.OUT_Threads, sizeof(DWORD));
	}
	// check the loaded value
	if (EncSettings.OUT_Threads < 1) EncSettings.OUT_Threads = 1;
	if (EncSettings.OUT_Threads > MAX_THREADS) EncSettings.OUT_Threads = MAX_THREADS;

	RegCloseKey(hKey);
	return 0;
}

