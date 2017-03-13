#include "stdafx.h"
#include "FLACdrop.h"
#include "io.h"
#include "Encoders.h"

// Global variables defined in FLACdrop.cpp
extern sEncoderSettings EncSettings;

//
//	FUNCTION:	About(HWND, UINT, WPARAM, LPARAM)
//
//	PURPOSE:	Message handler for about box.
//
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

//
//	FUNCTION:	Settings(HWND, UINT, WPARAM, LPARAM)
//
//	PURPOSE:	Message handler for settings box
//
INT_PTR CALLBACK Settings(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	HWND hDlgText = GetDlgItem(hDlg, IDC_MESSAGES);
	HWND hDlgFLACQuality = GetDlgItem(hDlg, IDC_FLAC_QUALITY);
	HWND hDlgFLACQualityView = GetDlgItem(hDlg, IDC_VIEW_FLAC_QUALITY);
	HWND hDlgFLACVerify = GetDlgItem(hDlg, IDC_FLAC_VERIFY);
	HWND hDlgFLACMD5check = GetDlgItem(hDlg, IDC_FLAC_MD5CHECK);
	HWND hDlgMP3InternalQuality = GetDlgItem(hDlg, IDC_MP3_ENCQ);
	HWND hDlgMP3InternalQualityView = GetDlgItem(hDlg, IDC_VIEW_MP3_INTERNAL_QUALITY);
	HWND hDlgMP3CBRBitrate = GetDlgItem(hDlg, IDC_MP3_BITRATE);
	HWND hDlgMP3VBRQuality = GetDlgItem(hDlg, IDC_MP3_VBR_Q);
	HWND hDlgMP3VBRQualityView = GetDlgItem(hDlg, IDC_VIEW_MP3_VBR_QUALITY);
	HWND hDlgThreads = GetDlgItem(hDlg, IDC_THREADS);
	HWND hDlgThreadsView = GetDlgItem(hDlg, IDC_VIEW_THREADS_NUMBER);
	TCHAR A[16];
	int result;

	switch (message)
	{
		case WM_INITDIALOG:
			// Setup FLAC encoding quality slider and number view
			SendMessage(hDlgFLACQuality, TBM_SETRANGE, false, MAKELONG(1, 8));
			SendMessage(hDlgFLACQuality, TBM_SETPOS, true, EncSettings.FLAC_EncodingQuality);
			_itow_s(EncSettings.FLAC_EncodingQuality, A, sizeof(A));
			SendMessage(hDlgFLACQualityView, WM_SETTEXT, NULL, (LPARAM)A);
			
			// Setup FLAC verify checkbox
			if(EncSettings.FLAC_Verify == true) SendMessage(hDlgFLACVerify, BM_SETCHECK, BST_CHECKED, 0);
			else SendMessage(hDlgFLACVerify, BM_SETCHECK, BST_UNCHECKED, 0);
			
			//Setup FLAC MD5 checkbox
			if(EncSettings.FLAC_MD5check == true) SendMessage(hDlgFLACMD5check, BM_SETCHECK, BST_CHECKED, 0);
			else SendMessage(hDlgFLACMD5check, BM_SETCHECK, BST_UNCHECKED, 0);

			// Setup LAME internal encoding quality slider and number view
			SendMessage(hDlgMP3InternalQuality, TBM_SETRANGE, false, MAKELONG(0, 9));
			SendMessage(hDlgMP3InternalQuality, TBM_SETPOS, true, EncSettings.LAME_InternalEncodingQuality);
			_itow_s(EncSettings.LAME_InternalEncodingQuality, A, sizeof(A));
			SendMessage(hDlgMP3InternalQualityView, WM_SETTEXT, NULL, (LPARAM)A);

			// Setup LAME VBR quality slider and number view
			SendMessage(hDlgMP3VBRQuality, TBM_SETRANGE, false, MAKELONG(0, 9));
			SendMessage(hDlgMP3VBRQuality, TBM_SETPOS, true, EncSettings.LAME_VBRQuality);
			_itow_s(EncSettings.LAME_VBRQuality, A, sizeof(A));
			SendMessage(hDlgMP3VBRQualityView, WM_SETTEXT, NULL, (LPARAM)A);

			// Fill up LAME combobox CBR bitrates
			memset(&A, 0, sizeof(A));
			for (int i = 0; i <= LAME_CBRBITRATES_QUANTITY; i++)
			{
				wcscpy_s(A, sizeof(A) / sizeof(TCHAR), (TCHAR*)LAME_CBRBITRATES_TEXT[i]);

				// Add string to combobox
				SendMessage(hDlgMP3CBRBitrate, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)A);
			}
			// Send the CB_SETCURSEL message to display an initial item in the selection field
			SendMessage(hDlgMP3CBRBitrate, CB_SETCURSEL, (WPARAM)EncSettings.LAME_CBRBitrate, (LPARAM)0);

			// Setup LAME MP3 output type
			switch (EncSettings.LAME_EncodingMode)
			{
				case 0:
					CheckRadioButton(hDlg, IDC_CBR, IDC_VBR, IDC_CBR);
					break;
				case 1:
					CheckRadioButton(hDlg, IDC_CBR, IDC_VBR, IDC_VBR);
					break;
			}
			
			// Setup thread number slider and number view
			SendMessage(hDlgThreads, TBM_SETRANGE, false, MAKELONG(1, MAX_THREADS));
			SendMessage(hDlgThreads, TBM_SETPOS, true, EncSettings.OUT_Threads);
			_itow_s(EncSettings.OUT_Threads, A, sizeof(A));
			SendMessage(hDlgThreadsView, WM_SETTEXT, NULL, (LPARAM)A);

			// Setup output type radio buttons
			switch (EncSettings.OUT_Type)
			{
				case 0:
					CheckRadioButton(hDlg, IDC_RADIO_FLAC, IDC_RADIO_MP3, IDC_RADIO_FLAC);
					break;

				case 1:
					CheckRadioButton(hDlg, IDC_RADIO_FLAC, IDC_RADIO_MP3, IDC_RADIO_MP3);
					break;
			}

			return (INT_PTR)TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDCANCEL:
					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;

				case IDOK:
					EncSettings.FLAC_EncodingQuality = (int)SendMessage(hDlgFLACQuality, TBM_GETPOS, 0, 0);
					if(SendMessage(hDlgFLACVerify, BM_GETCHECK, 0, 0) == BST_CHECKED) EncSettings.FLAC_Verify = true;
					else EncSettings.FLAC_Verify = false;
					if(SendMessage(hDlgFLACMD5check, BM_GETCHECK, 0, 0) == BST_CHECKED) EncSettings.FLAC_MD5check = true;
					else EncSettings.FLAC_MD5check = false;
					EncSettings.LAME_InternalEncodingQuality = (int)SendMessage(hDlgMP3InternalQuality, TBM_GETPOS, 0, 0);
					EncSettings.LAME_CBRBitrate = (int)SendMessage(hDlgMP3CBRBitrate, CB_GETCURSEL, 0, 0);
					EncSettings.LAME_VBRQuality = (int)SendMessage(hDlgMP3VBRQuality, TBM_GETPOS, 0, 0);
					EncSettings.OUT_Threads = (int)SendMessage(hDlgThreads, TBM_GETPOS, 0, 0);

					// Read status of radio buttons
					if (IsDlgButtonChecked(hDlg, IDC_RADIO_FLAC) == BST_CHECKED) EncSettings.OUT_Type = 0;
					if (IsDlgButtonChecked(hDlg, IDC_RADIO_MP3) == BST_CHECKED) EncSettings.OUT_Type = 1;
					if (IsDlgButtonChecked(hDlg, IDC_CBR) == BST_CHECKED) EncSettings.LAME_EncodingMode = 0;
					if (IsDlgButtonChecked(hDlg, IDC_VBR) == BST_CHECKED) EncSettings.LAME_EncodingMode = 1;

					result = RegOut();
					if (result!= 0) SendMessage(hDlgText, WM_SETTEXT, 0, (LPARAM)ErrMessage[result]);

					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;
			}
			break;
		
		case WM_HSCROLL:
			// one of the sliders got moved so we get the slider control positions and write the positions on the screen
			result = SendMessage(hDlgFLACQuality, TBM_GETPOS, 0, 0);
			_itow_s(result, A, sizeof(A));
			SendMessage(hDlgFLACQualityView, WM_SETTEXT, NULL, (LPARAM)A);
			
			result = SendMessage(hDlgMP3InternalQuality, TBM_GETPOS, 0, 0);
			_itow_s(result, A, sizeof(A));
			SendMessage(hDlgMP3InternalQualityView, WM_SETTEXT, NULL, (LPARAM)A);
			
			result = SendMessage(hDlgMP3VBRQuality, TBM_GETPOS, 0, 0);
			_itow_s(result, A, sizeof(A));
			SendMessage(hDlgMP3VBRQualityView, WM_SETTEXT, NULL, (LPARAM)A);
			
			result = SendMessage(hDlgThreads, TBM_GETPOS, 0, 0);
			_itow_s(result, A, sizeof(A));
			SendMessage(hDlgThreadsView, WM_SETTEXT, NULL, (LPARAM)A);
			return (INT_PTR)TRUE;
	}
	return (INT_PTR)FALSE;
}

//
//	FUNCTION:	MainForm(HWND, UINT, WPARAM, LPARAM)
//
//	PURPOSE:	Handle the "drag and drop" audio files and pass them to the encoder scheduler
//
INT_PTR CALLBACK MainForm(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	static sUIParameters UIParameters;
	int result;

	switch (message)
	{
		case WM_INITDIALOG:
			UIParameters.EncoderInUse = false;
			UIParameters.progress[0] = GetDlgItem(hDlg, IDC_PROGRESS0);
			UIParameters.progress[1] = GetDlgItem(hDlg, IDC_PROGRESS1);
			UIParameters.progress[2] = GetDlgItem(hDlg, IDC_PROGRESS2);
			UIParameters.progress[3] = GetDlgItem(hDlg, IDC_PROGRESS3);
			UIParameters.progress[4] = GetDlgItem(hDlg, IDC_PROGRESS4);
			UIParameters.progress[5] = GetDlgItem(hDlg, IDC_PROGRESS5);
			UIParameters.progress[6] = GetDlgItem(hDlg, IDC_PROGRESS6);
			UIParameters.progress[7] = GetDlgItem(hDlg, IDC_PROGRESS7);
			UIParameters.progresstotal = GetDlgItem(hDlg, IDC_PROGRESSTOTAL);
			UIParameters.text = GetDlgItem(hDlg, IDC_MESSAGES);
			SendMessage(UIParameters.text, WM_SETTEXT, 0, (LPARAM)L"Waiting for dropped WAV or FLAC files...");
			
			result = RegIn();			// load the encoder settings
			if (result != 0) SendMessage(UIParameters.text, WM_SETTEXT, 0, (LPARAM)ErrMessage[result]);
			return (INT_PTR)TRUE;

		// process the dropped files
		case WM_DROPFILES:
			if (UIParameters.EncoderInUse == false)	// check if the encoder thread is already running
			{
				UIParameters.EncoderInUse = true;
				UIParameters.filedrop = (HDROP)wParam;
				CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&EncoderScheduler, &UIParameters, 0, NULL);	// start the encoder thread
			}
			break;
	}
	return (INT_PTR)FALSE;
}