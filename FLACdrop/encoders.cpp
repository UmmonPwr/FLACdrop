#include "stdafx.h"
#include "FLACdrop.h"
#include "encoders.h"
#include "lame.h"
#include "libFLAC_callbacks.h"

extern sEncoderSettings EncSettings;			// variable to store encoder settings
extern TCHAR *EventLogTXT;						// variable to store event log history
HANDLE ghSemaphore;								// handle for the semaphore

//
//	FUNCTION: SearchFreeThread()
//
//	PURPOSE: Search for a free slot in the thread data block
//
int SearchFreeThread(sEncodingParameters EncParams[])
{
	int i = 0;

	while (EncParams[i].ThreadInUse == true) i++;

	return i;
}

//
//	FUNCTION:	EncoderScheduler(sUIParameters* )
//
//	PURPOSE:	Collects the dropped files list and schedules the encoding threads
//
DWORD WINAPI EncoderScheduler(LPVOID *params)
{
	UINT NumFiles, BufferSize;
	static HANDLE aThread[MAX_THREADS];																// array for the thread identifiers
	sUIParameters *myparams =(sUIParameters*)params;												// load the parameter list to the internal structure
	static sEncodingParameters EncParams[MAX_THREADS];												// parameter list for each encoding thread
	WCHAR Filename[MAXFILENAMELENGTH];
	WCHAR *FilenameExt;
	int tID = 0;																					// actual thread id
	bool ThreadStarted;																				// was a thread started within the loop?
	
	NumFiles = DragQueryFile(myparams->filedrop, 0xFFFFFFFF, NULL, 0);								// get the number of dropped files
	SendMessage(myparams->progresstotal, PBM_SETPOS, 0, 0);											// reset the total progress bar
	for (int i = 0; i<MAX_THREADS; i++) SendMessage(myparams->progress[i], PBM_SETPOS, 0, 0);		// reset each thread's progress bar
	SendMessage(myparams->progresstotal, PBM_SETRANGE, 0, MAKELONG(0, NumFiles));					// set the total progress bar boundaries
	SendMessage(myparams->text, WM_SETTEXT, 0, (LPARAM)L"Converting is in progress");

	// number of the semaphore is the number of threads we would like to use in parallel
	// if the number of files are less than the number of available threads then we have to create the semaphore for the number of files
	if (EncSettings.OUT_Threads > NumFiles) ghSemaphore = CreateSemaphore(NULL, NumFiles - 1, MAX_THREADS, NULL);
	else ghSemaphore = CreateSemaphore(NULL, EncSettings.OUT_Threads-1, MAX_THREADS, NULL);
	
	// setup the variales
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].ThreadInUse = false;								// reset the thread status flags
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].text = myparams->text;
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].progresstotal = myparams->progresstotal;

	for(UINT i=0; i < NumFiles; i++)
	{
		ThreadStarted = false;
		BufferSize = DragQueryFile(myparams->filedrop, i, NULL, 0);									// get the size of the file name buffer of the i(th) dropped file
		if(BufferSize == 0) break;																	// something is wrong
		DragQueryFile(myparams->filedrop, i, Filename, BufferSize + 1);								// retrieve the name of the i(th) dropped file
		
		// check if the extension in the filename is WAV or FLAC and start encoder algorithm accordingly
		wchar_t ch=L'.';
		FilenameExt = wcsrchr(Filename, ch);	// search for the last "." in the filename

		// Source is WAVE file
		if (_wcsnicmp(FilenameExt, L".wav", 4) == 0)
		{			
			switch (myparams->OUT_Type)
			{
				case OUT_TYPE_FLAC:
					tID = SearchFreeThread(EncParams);
					EncParams[tID].ThreadInUse = true;
					EncParams[tID].progress = myparams->progress[tID];
					wcscpy_s(EncParams[tID].filename, MAXFILENAMELENGTH, Filename);
					aThread[tID] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&Encode_WAV2FLAC, &EncParams[tID], 0, NULL);
					ThreadStarted = true;
					break;

				case OUT_TYPE_MP3:
					tID = SearchFreeThread(EncParams);
					EncParams[tID].ThreadInUse = true;
					EncParams[tID].progress = myparams->progress[tID];
					wcscpy_s(EncParams[tID].filename, MAXFILENAMELENGTH, Filename);
					aThread[tID] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&Encode_WAV2MP3, &EncParams[tID], 0, NULL);
					ThreadStarted = true;
					break;
			}
		}

		// Source is FLAC file
		if (_wcsnicmp(FilenameExt, L".flac", 5) == 0)
		{
			switch (myparams->OUT_Type)
			{
				case OUT_TYPE_WAV:
					tID = SearchFreeThread(EncParams);
					EncParams[tID].ThreadInUse = true;
					EncParams[tID].progress = myparams->progress[tID];
					wcscpy_s(EncParams[tID].filename, MAXFILENAMELENGTH, Filename);
					aThread[tID] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&Encode_FLAC2WAV, &EncParams[tID], 0, NULL);
					ThreadStarted = true;
					break;
				
				case OUT_TYPE_MP3:
					tID = SearchFreeThread(EncParams);
					EncParams[tID].ThreadInUse = true;
					EncParams[tID].progress = myparams->progress[tID];
					wcscpy_s(EncParams[tID].filename, MAXFILENAMELENGTH, Filename);
					aThread[tID] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&Encode_FLAC2MP3, &EncParams[tID], 0, NULL);
					ThreadStarted = true;
					break;
			}
		}
		
		if (ThreadStarted == true) WaitForSingleObject(ghSemaphore, INFINITE);	// a thread was started so we have to decrease the count of the semaphore with one, continue only if at least one thread is free
		else SendMessage(myparams->progresstotal, PBM_DELTAPOS, 1, 0);			// no thread was started (file extension was not recognized), but we still have to increase the total progress bar
	}

	// wait for all threads to terminate
	if (EncSettings.OUT_Threads > NumFiles) WaitForMultipleObjects(NumFiles, aThread, TRUE, INFINITE);
	else WaitForMultipleObjects(EncSettings.OUT_Threads, aThread, TRUE, INFINITE);

	// release the memory which the system allocated for the file name transfer
	DragFinish(myparams->filedrop);
	myparams->EncoderInUse = false;

	CloseHandle(ghSemaphore);
	if (EncSettings.OUT_Threads > NumFiles) for (UINT i = 0; i<NumFiles; i++) CloseHandle(aThread[i]);
	else for (UINT i = 0; i<EncSettings.OUT_Threads; i++) CloseHandle(aThread[i]);
	
	SendMessage(myparams->text, WM_SETTEXT, 0, (LPARAM)L"Waiting for audio files to be dropped...");

	return ALL_OK;		// only to prevent compiler warning message
}

//
//	FUNCTION:	ExitEncThread(int, HANDLE, HWND)
//
//	PURPOSE:	Exits the encoder thread, releases the semaphore and updates event log
//
void ExitEncThread(int ExitCode, HANDLE Semaphore, HWND progresstotal, WCHAR *filename, int type)
{
	WCHAR rn[] = L"\r\n";
	WCHAR FLAC[] = L"Output type: FLAC\r\n";
	WCHAR MP3[] = L"Output type: MP3\r\n";
	WCHAR WAV[] = L"Output type: WAV\r\n";

	wcscat_s(EventLogTXT, EVENTLOGSIZE, filename);
	wcscat_s(EventLogTXT, EVENTLOGSIZE, rn);

	switch (type)
	{
		case OUT_TYPE_FLAC:
			wcscat_s(EventLogTXT, EVENTLOGSIZE, FLAC);
			break;
		case OUT_TYPE_MP3:
			wcscat_s(EventLogTXT, EVENTLOGSIZE, MP3);
			break;
		case OUT_TYPE_WAV:
			wcscat_s(EventLogTXT, EVENTLOGSIZE, WAV);
			break;
	}
	wcscat_s(EventLogTXT, EVENTLOGSIZE, ErrMessage[ExitCode]);
	wcscat_s(EventLogTXT, EVENTLOGSIZE, rn);

	SendMessage(progresstotal, PBM_DELTAPOS, 1, 0);						// increase the total progress bar, we have finished with the encoding even if there was an error event
	ReleaseSemaphore(Semaphore, 1, NULL);
	//ExitThread(ExitCode);
}

//
//	FUNCTION:	Encode_WAV2MP3(sEncodingParameters* )
//
//	PURPOSE:	Encode WAV stream to MP3 stream
//
DWORD WINAPI Encode_WAV2MP3(LPVOID *params)
{
	sEncodingParameters *myparams = (sEncodingParameters*)params;
	sWAVEheader WAVEheader;
	sFMTheader FMTheader;
	sDATAheader DATAheader;

	lame_global_flags *lame_gfp;
	FILE *fin, *fout;
	unsigned int total_samples = 0;	// can use a 32-bit number due to WAV file size limitation
	int err = 0;


	// WAV: open the input WAVE file
	{
		if ((_wfopen_s(&fin, myparams->filename, L"rb")) != NULL)
		{
			err = FAIL_FILE_OPEN;
		}
	}

	// WAV: read wav header and check if it is valid
	if (err == ALL_OK)
	{
		if (fread(&WAVEheader, 1, 12, fin) != 12)
		{
			fclose(fin);
			err = FAIL_FILE_OPEN;
		}
	}
	if (err == ALL_OK)
	{
		if (memcmp(WAVEheader.ChunkID, "RIFF", 4) || memcmp(WAVEheader.Format, "WAVE", 4))
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}

	// WAV: read the format chunk's header only for its chunk size
	if (err == ALL_OK)
	{
		if (fread(&DATAheader, 1, 8, fin) != 8)
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}

	// WAV: read the complete wave file header according to its actual chunk size (16, 18 or 40 byte), ChunkSize does not include the size of the header
	if (err == ALL_OK)
	{
		fseek(fin, -8, SEEK_CUR);
		if (fread(&FMTheader, 1, (size_t)DATAheader.ChunkSize + 8, fin) != (size_t)DATAheader.ChunkSize + 8)
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}

	// WAV: check if the wav file has PCM uncompressed data
	if (err == ALL_OK)
	{
		switch (FMTheader.AudioFormat)
		{
		case WAVE_FORMAT_PCM:
			break;
		case WAVE_FORMAT_EXTENSIBLE:
			// in this case the first two byte of the SubFormat is defining the audio format
			if (FMTheader.SubFormat_AudioFormat != WAVE_FORMAT_PCM)
			{
				fclose(fin);
				err = FAIL_WAV_UNSUPPORTED;
			}
			break;
		default:
			fclose(fin);
			err = FAIL_WAV_UNSUPPORTED;
		}
	}

	// WAV: check if the WAVE file has 16 bit resolution, MP3 stream does not support 24 bit
	if (err == ALL_OK)
	{
		switch (FMTheader.BitsPerSample)
		{
		case 16:
			break;
		case 24:
		default:
			fclose(fin);
			err = FAIL_LAME_ONLY_16_BIT;
		}
	}

	// WAV: search for the data chunk
	if (err == ALL_OK)
	{
		do
		{
			fread(&DATAheader, 1, 8, fin);
			fseek(fin, DATAheader.ChunkSize, SEEK_CUR);
		} while (memcmp(DATAheader.ChunkID, "data", 4));
		fseek(fin, -DATAheader.ChunkSize, SEEK_CUR);													// go back to the beginning of the data chunk
		total_samples = DATAheader.ChunkSize / FMTheader.NumChannels / (FMTheader.BitsPerSample / 8);	// sound data's size divided by one sample's size
	}
	
	// libmp3lame: initialize lame encoder
	if (err == ALL_OK)
	{
		lame_gfp = lame_init();
		if (lame_gfp == NULL)
		{
			fclose(fin);
			err = FAIL_LAME_INIT;
		}
	}
	
	// libmp3lame: set encoder parameters
	if (err == ALL_OK)
	{
		switch (FMTheader.NumChannels)
		{
		case 1:
			lame_set_mode(lame_gfp, MONO);
			break;
		case 2:
			lame_set_mode(lame_gfp, JOINT_STEREO);
			break;
		default:
			// only mono and stereo streams are supported by libmp3lame
			fclose(fin);
			err = FAIL_LAME_MAX_2_CHANNEL;
			break;
		}
	}
	if (err == ALL_OK)
	{
		// Internal algorithm selection. True quality is determined by the bitrate but this variable will effect quality by selecting expensive or cheap algorithms.
		// quality=0..9.  0=best (very slow).  9=worst.
		// recommended:  2     near-best quality, not too slow
		// 5     good quality, fast
		// 7     ok quality, really fast
		lame_set_quality(lame_gfp, EncSettings.LAME_InternalEncodingQuality);

		// turn off automatic writing of ID3 tag data into mp3 stream we have to call it before 'lame_init_params', because that function would spit out ID3v2 tag data.
		lame_set_write_id3tag_automatic(lame_gfp, 0);

		// set lame encoder parameters for CBR encoding
		lame_set_num_channels(lame_gfp, FMTheader.NumChannels);
		lame_set_in_samplerate(lame_gfp, FMTheader.SampleRate);
		lame_set_brate(lame_gfp, LAME_CBRBITRATES[EncSettings.LAME_CBRBitrate]);	// load encoding bitrate setting from LUT

		// set lame encoder parameters for VBR encoding
		switch (EncSettings.LAME_EncodingMode)
		{
		case 0:	// CBR
			lame_set_VBR(lame_gfp, vbr_off);
			break;
		case 1:	// VBR
			lame_set_VBR(lame_gfp, vbr_mtrh);
			break;
		}

		lame_set_VBR_q(lame_gfp, EncSettings.LAME_VBRQuality); // VBR quality level.  0=highest  9=lowest

		// now that all the options are set, lame needs to analyze them and set some more internal options and check for problems
		if (lame_init_params(lame_gfp) != 0)
		{
			fclose(fin);
			err = FAIL_LAME_INIT;
		}
	}

	// libmp3lame: open output file
	if (err == ALL_OK)
	{
		WCHAR* OutFileName;

		OutFileName = new WCHAR[MAXFILENAMELENGTH];
		wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH) - 3);	// leave out the "wav" from the end
		wcscat_s(OutFileName, MAXFILENAMELENGTH, L"mp3");
		if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
		{
			fclose(fin);
			err = FAIL_FILE_OPEN;
		}
		delete[]OutFileName;
	}

	// libmp3lame: start encoding
	if (err == ALL_OK)
	{
		size_t left, need;
		BYTE* buffer_mp3, * buffer_wav;
		int imp3, owrite;
		bool ok = true;

		// set up the progress bar boundaries
		SendMessage(myparams->progress, PBM_SETPOS, 0, 0);
		SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, total_samples / READSIZE_MP3));

		// allocate memory buffers
		buffer_wav = new BYTE[READSIZE_MP3 * FMTheader.NumChannels * (FMTheader.BitsPerSample / 8)];
		buffer_mp3 = new BYTE[LAME_MAXMP3BUFFER];

		/*size_t  id3v2_size;
		unsigned char *id3v2tag;

		id3v2_size = lame_get_id3v2_tag(lame_gfp, 0, 0);
		if (id3v2_size > 0)
		{
			id3v2tag = new unsigned char[id3v2_size];
			if (id3v2tag != 0)
			{
				imp3 = lame_get_id3v2_tag(lame_gfp, id3v2tag, id3v2_size);
				owrite = (int) fwrite(id3v2tag, 1, imp3, fout);
				delete []id3v2tag;
				if (owrite != imp3) return FAIL_LAME_ID3TAG;
			}
		}
		else
		{
			unsigned char* id3v2tag = getOldTag(gf);
			id3v2_size = sizeOfOldTag(gf);
			if ( id3v2_size > 0 )
			{
				size_t owrite = fwrite(id3v2tag, 1, id3v2_size, fout);
				if (owrite != id3v2_size) return FAIL_LAME_ID3TAG;
			}
		}
		if (LAME_FLUSH == true) fflush(fout);*/

		left = (size_t)total_samples;
		// read blocks of samples from WAVE file and feed to the encoder
		while (ok && left)
		{
			need = (left > READSIZE_MP3 ? (size_t)READSIZE_MP3 : (size_t)left);	// calculate the number of samples to read
			if (fread(buffer_wav, (size_t)FMTheader.NumChannels * (FMTheader.BitsPerSample / 8), need, fin) != need)
			{
				// error during reading from WAVE file
				ok = false;
			}
			else
			{
				// feed samples to the encoder
				switch (FMTheader.NumChannels)
				{
				case 2:
					imp3 = lame_encode_buffer_interleaved(lame_gfp, (short int*)buffer_wav, need, buffer_mp3, LAME_MAXMP3BUFFER);
					break;
				case 1:	// the interleaved version corrupts the mono stream
					imp3 = lame_encode_buffer(lame_gfp, (short int*)buffer_wav, NULL, need, buffer_mp3, LAME_MAXMP3BUFFER);
					break;
				}

				// was our output buffer big enough?
				if (imp3 < 0) ok = false;
				else
				{
					owrite = (int)fwrite(buffer_mp3, 1, imp3, fout);
					if (owrite != imp3) ok = false;
					if (LAME_FLUSH == true) fflush(fout);
				}

				SendMessage(myparams->progress, PBM_DELTAPOS, 1, 0);	// increase the progress bar
			}
			left -= need;
		}

		// may return one more mp3 frame
		if (ok == true)
		{
			if (LAME_NOGAP == true) imp3 = lame_encode_flush_nogap(lame_gfp, buffer_mp3, LAME_MAXMP3BUFFER);
			else imp3 = lame_encode_flush(lame_gfp, buffer_mp3, LAME_MAXMP3BUFFER);
			if (imp3 < 0) ok = false;
		}

		if (ok == true)
		{
			owrite = (int)fwrite(buffer_mp3, 1, imp3, fout);
			if (owrite != imp3) ok = false;
		}

		if (LAME_FLUSH == true && ok == true) fflush(fout);

		delete[]buffer_wav;
		delete[]buffer_mp3;

		if (ok == false)
		{
			fclose(fin);
			fclose(fout);
			err = FAIL_LAME_ENCODE;
		}
	}

	// libmp3lame: close encoder
	if (err == ALL_OK)
	{
		if (lame_close(lame_gfp) != 0) err = FAIL_LAME_CLOSE;
		fclose(fin);
		fclose(fout);
	}

	myparams->ThreadInUse = false;
	ExitEncThread(err, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	return ALL_OK;
}

//
//	FUNCTION:	Encode_WAV2FLAC(sEncodingParameters* )
//
//	PURPOSE:	Encode WAV stream to FLAC stream
//
DWORD WINAPI Encode_WAV2FLAC(LPVOID *params)
{
	sEncodingParameters *myparams = (sEncodingParameters*)params;
	FLAC__StreamEncoder *encoder = NULL;
	unsigned int total_samples = 0;		// can use a 32-bit number due to WAV file size limitations in specification
	FILE* fin, * fout;
	int err = 0;

	sWAVEheader WAVEheader;
	sFMTheader FMTheader;
	sDATAheader DATAheader;
	sClientData ClientData;
	
	// WAV: open file
	{
		if ((_wfopen_s(&fin, myparams->filename, L"rb")) != NULL)
			err = FAIL_FILE_OPEN;
	}
	
	// WAV: read wav header and check if it is valid
	if (err == ALL_OK)
	{
		if (fread(&WAVEheader, 1, 12, fin) != 12)
		{
			fclose(fin);
			err = FAIL_FILE_OPEN;
		}
		if (memcmp(WAVEheader.ChunkID, "RIFF", 4) || memcmp(WAVEheader.Format, "WAVE", 4))
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}

	// WAV: read the format chunk's header only to get its chunk size
	if (err == ALL_OK)
	{
		if (fread(&DATAheader, 1, 8, fin) != 8)
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}
	
	// WAV: read the complete wave file header according to its actual chunk size (16, 18 or 40 byte), ChunkSize does not include the size of the header
	if (err == ALL_OK)
	{
		fseek(fin, -8, SEEK_CUR);
		if (fread(&FMTheader, 1, (size_t)DATAheader.ChunkSize + 8, fin) != (size_t)DATAheader.ChunkSize + 8)
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}

	// WAV: check if the wav file has PCM uncompressed data
	if (err == ALL_OK)
	{
		switch (FMTheader.AudioFormat)
		{
			case WAVE_FORMAT_PCM:
				break;
			case WAVE_FORMAT_EXTENSIBLE:
				// in this case the first two byte of the SubFormat is defining the audio format
				if (FMTheader.SubFormat_AudioFormat != WAVE_FORMAT_PCM)
				{
					fclose(fin);
					err = FAIL_WAV_UNSUPPORTED;
				}
				break;
			default:
				fclose(fin);
				err = FAIL_WAV_UNSUPPORTED;
		}
	}

	// WAV: check if the WAVE file has 16 or 24 bit resolution
	if (err == ALL_OK)
	{
		switch (FMTheader.BitsPerSample)
		{
		case 16:
		case 24:
			break;
		default:
			fclose(fin);
			err = FAIL_LIBFLAC_ONLY_16_24_BIT;
		}
	}

	// WAV: search for the data chunk
	if (err == ALL_OK)
	{
		do
		{
			fread(&DATAheader, 1, 8, fin);
			fseek(fin, DATAheader.ChunkSize, SEEK_CUR);
		} while (memcmp(DATAheader.ChunkID, "data", 4));
		fseek(fin, -DATAheader.ChunkSize, SEEK_CUR);													// go back to the beginning of the data chunk
		total_samples = DATAheader.ChunkSize / FMTheader.NumChannels / (FMTheader.BitsPerSample / 8);	// sound data's size divided by one sample's size
	}
   
	// libFLAC: allocate the libFLAC encoder and data buffers
	if (err == ALL_OK)
	{
		if ((encoder = FLAC__stream_encoder_new()) == NULL)
		{
			fclose(fin);
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: set the encoder parameters
	if (err == ALL_OK)
	{
		FLAC__bool ok = true;

		ok &= FLAC__stream_encoder_set_verify(encoder, EncSettings.FLAC_Verify);
		ok &= FLAC__stream_encoder_set_compression_level(encoder, EncSettings.FLAC_EncodingQuality);
		ok &= FLAC__stream_encoder_set_channels(encoder, FMTheader.NumChannels);
		ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, FMTheader.BitsPerSample);
		ok &= FLAC__stream_encoder_set_sample_rate(encoder, FMTheader.SampleRate);
		ok &= FLAC__stream_encoder_set_total_samples_estimate(encoder, total_samples);

		if (ok == false)
		{
			fclose(fin);
			FLAC__stream_encoder_delete(encoder);
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: now add some metadata; we'll add some tags and a padding block
//	if(err == ALL_OK)
//	{
//		FLAC__StreamMetadata *metadata[2];
//		FLAC__StreamMetadata_VorbisComment_Entry entry;

//		if((metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT)) == NULL ||
//			(metadata[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)) == NULL ||
			// there are many tag (vorbiscomment) functions but these are convenient for this particular use:
//			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ARTIST", "Some Artist") ||
//			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false) || // copy=false: let metadata object take control of entry's allocated string
//			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "YEAR", "1984") ||
//			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false))
//		{
			// out of memory or tag error
//			ok = false;
//			err = FAIL_LIBFLAC_METADATA;
//		}
//		metadata[1]->length = 1234;	// set the padding length
//		ok = FLAC__stream_encoder_set_metadata(encoder, metadata, 2);
//	}

	// libFLAC: initialize the libFLAC encoder
	if(err == ALL_OK)
	{
		WCHAR* OutFileName;

		OutFileName = new WCHAR[MAXFILENAMELENGTH];
		wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH)-3);	// leave out the "wav" from the end
		wcscat_s(OutFileName, MAXFILENAMELENGTH, L"flac");
		if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
		{
			fclose(fin);
			err = FAIL_FILE_OPEN;
		}
		else
		{
			ClientData.fout = fout;
			if (FLAC__stream_encoder_init_stream(encoder, write_callback_2FLAC, seek_callback_2FLAC, tell_callback_2FLAC, metadata_callback_2FLAC, &ClientData))
			{
				fclose(fin);
				fclose(fout);
				FLAC__stream_encoder_delete(encoder);
				err = FAIL_LIBFLAC_ALLOC;
			}
		}

		delete[]OutFileName;
	}

	// libFLAC: read blocks of samples from WAVE file and feed to the encoder
	if(err == ALL_OK)
	{
		size_t left, need, i;
		FLAC__byte *buffer_wav;				// READSIZE * bytes per sample * channels, we read the WAVE data into here
		FLAC__int32 *buffer_flac;			// READSIZE * channels
		bool ok = true;

		// set up the progress bar boundaries and reset it
		SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, total_samples / READSIZE_FLAC));
		SendMessage(myparams->progress, PBM_SETPOS, 0, 0);

		buffer_wav = new FLAC__byte[READSIZE_FLAC * FMTheader.NumChannels * (FMTheader.BitsPerSample / 8)];
		buffer_flac = new FLAC__int32[READSIZE_FLAC * FMTheader.NumChannels];

		left = (size_t)total_samples;
		while(ok && left)
		{
			need = (left>READSIZE_FLAC? (size_t)READSIZE_FLAC : (size_t)left);	// calculate the number of samples to read
			if(fread(buffer_wav, (size_t)FMTheader.NumChannels * (FMTheader.BitsPerSample/8), need, fin) != need)
			{
				// error during reading from WAVE file
				ok = false;
			}
			else
			{
				// convert the packed little-endian PCM samples from WAVE file into an interleaved FLAC__int32 buffer for libFLAC
				switch(FMTheader.BitsPerSample)
				{
					case 16:
						for(i = 0; i < need * FMTheader.NumChannels; i++)
						{
							// convert the 16 bit values stored in byte array into 32 bit signed integer values
							buffer_flac[i] = buffer_wav[2*i+1] << 8;
							buffer_flac[i] |= buffer_wav[2*i];
							if (buffer_flac[i] & 0x8000) buffer_flac[i] |= 0xffff0000; // correct the top 16 bit to have correct 2nd complement code
						}
						break;
					case 24:
						for(i = 0; i < need * FMTheader.NumChannels; i++)
						{
							// convert the 24 bit values stored in byte array into 32 bit signed integer values
							buffer_flac[i] = buffer_wav[3*i+2] << 16;
							buffer_flac[i] |= buffer_wav[3*i+1] << 8;
							buffer_flac[i] |= buffer_wav[3*i];
							if (buffer_flac[i] & 0x800000) buffer_flac[i] |= 0xff000000; // correct the top 8 bit to have correct 2nd complement code
						}
						break;
				}

				// feed samples to the encoder
				ok = FLAC__stream_encoder_process_interleaved(encoder, buffer_flac, need);
				SendMessage(myparams->progress, PBM_DELTAPOS, 1, 0);	// increase the progress bar
			}
			left -= need;
		}
		
		delete[]buffer_wav;
		delete[]buffer_flac;
	}

	// libFLAC: close the encoder
	if (err == ALL_OK)
	{
		if (FLAC__stream_encoder_finish(encoder) == false) err = FAIL_LIBFLAC_RELEASE;
		// now that encoding is finished, the metadata can be freed
		//FLAC__metadata_object_delete(metadata[0]);
		//FLAC__metadata_object_delete(metadata[1]);
		fclose(fin);
		fclose(fout);
		FLAC__stream_encoder_delete(encoder);
	}

	myparams->ThreadInUse = false;
	ExitEncThread(err, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	return ALL_OK;
}

//
//	FUNCTION:	Encode_FLAC2WAV(sEncodingParameters* )
//
//	PURPOSE:	Encode FLAC stream to WAV stream
//
DWORD WINAPI Encode_FLAC2WAV(LPVOID* params)
{
	sEncodingParameters* myparams = (sEncodingParameters*)params;
	FLAC__StreamDecoder* decoder = 0;
	FILE* fin, * fout;
	int err = 0;
	sClientData ClientData;

	// libFLAC: allocate the decoder
	{
		if ((decoder = FLAC__stream_decoder_new()) == NULL)
		{
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: set the decoder parameters
	if (err == ALL_OK)
	{
		FLAC__stream_decoder_set_md5_checking(decoder, EncSettings.FLAC_MD5check);
	}

	// libFLAC: open the input file
	if (err == ALL_OK)
	{
		if ((_wfopen_s(&fin, myparams->filename, L"r+b")) != NULL)
		{
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_FILE_OPEN;
		}
		else ClientData.fin = fin;
	}

	// libFLAC: initialize decoder
	if (err == ALL_OK)
	{
		if (FLAC__stream_decoder_init_stream(decoder, read_callback_2WAV, seek_callback_2WAV, tell_callback_2WAV, length_callback_2WAV, eof_callback_2WAV, write_callback_2WAV, metadata_callback_2WAV, error_callback_2WAV, &ClientData) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: check the FLAC file parameters
	if (err == ALL_OK)
	{
		// check if the first block really contained the metadata
		if (FLAC__stream_decoder_process_until_end_of_metadata(decoder) == FALSE)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_BAD_HEADER;
		}
	}
	if (err == ALL_OK)
	{
		// check if FLAC file has 16 or 24 bit resolution
		switch (ClientData.bps)
		{
		case 16:
		case 24:
			break;
		default:
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_ONLY_16_24_BIT;
		}
	}
	if (err == ALL_OK)
	{
		// check if total_samples count is in the STREAMINFO
		if (ClientData.total_samples == 0)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_BAD_HEADER;
		}
	}

	// libFLAC: open the output file
	if (err == ALL_OK)
	{
		WCHAR* OutFileName;

		OutFileName = new WCHAR[MAXFILENAMELENGTH];
		wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH) - 4);	// leave out the "flac" from the end
		wcscat_s(OutFileName, MAXFILENAMELENGTH, L"wav");
		if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_FILE_OPEN;
		}
		ClientData.fout = fout;

		delete[]OutFileName;
	}

	// libFLAC: start the decoding
	if (err == ALL_OK)
	{
		FLAC__bool ok = TRUE;
		FLAC__StreamDecoderState state;

		// set up the progress bar boundaries, block size is around 4k depending on resolution
		SendMessage(myparams->progress, PBM_SETPOS, 0, 0);
		SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, ClientData.total_samples / ClientData.blocksize));
		
		// loop the decoder until it reaches the end of the input file or returns an error
		do
		{
			ok = FLAC__stream_decoder_process_single(decoder);
			state = FLAC__stream_decoder_get_state(decoder);
			SendMessage(myparams->progress, PBM_DELTAPOS, 1, 0);	// increase the progress bar
		} while ((state != FLAC__STREAM_DECODER_END_OF_STREAM && FLAC__STREAM_DECODER_SEEK_ERROR && FLAC__STREAM_DECODER_ABORTED && FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) && ok == TRUE);
	}

	// libFLAC: close the decoder
	if (err == ALL_OK)
	{
		switch (FLAC__stream_decoder_get_state(decoder))
		{
		case FLAC__STREAM_DECODER_SEEK_ERROR:
		case FLAC__STREAM_DECODER_ABORTED:
		case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
			fclose(fout);
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_ENCODE;
			break;
		default:
			break;
		}
	}
	if (err == ALL_OK)
	{
		if (FLAC__stream_decoder_finish(decoder) == FALSE) err = WARN_LIBFLAC_MD5;
		fclose(fout);
		fclose(fin);
		FLAC__stream_decoder_delete(decoder);
	}

	myparams->ThreadInUse = false;
	ExitEncThread(err, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);
	return ALL_OK;
}

//
//	FUNCTION:	Encode_FLAC2MP3(sEncodingParameters* )
//
//	PURPOSE:	Encode FLAC stream to MP3 stream, transfer the tags also
//
DWORD WINAPI Encode_FLAC2MP3(LPVOID* params)
{
	sEncodingParameters* myparams = (sEncodingParameters*)params;
	sMetaData MetaDataTrans[MD_NUMBER];
	sClientData ClientData;

	FILE* fin, * fout;
	FLAC__StreamDecoder* decoder = 0;
	lame_global_flags* lame_gfp;
	int err = 0;

	// libFLAC: allocate the libFLAC decoder
	{
		if ((decoder = FLAC__stream_decoder_new()) == NULL)
		{
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: set the decoder parameters
	if (err == ALL_OK)
	{
		FLAC__stream_decoder_set_md5_checking(decoder, EncSettings.FLAC_MD5check);
	}

	// libFLAC: open the input file and give the handle to the libFLAC decoder
	if (err == ALL_OK)
	{
		if ((_wfopen_s(&fin, myparams->filename, L"r+b")) != NULL)
		{
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_FILE_OPEN;
		}
		else ClientData.fin = fin;
	}

	// libFLAC: initialize decoder, write callback function is "_2MEM"
	if (err == ALL_OK)
	{
		if (FLAC__stream_decoder_init_stream(decoder, read_callback_2WAV, seek_callback_2WAV, tell_callback_2WAV, length_callback_2WAV, eof_callback_2WAV, write_callback_2MEM, metadata_callback_2WAV, error_callback_2WAV, &ClientData) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: check the FLAC file parameters
	if (err == ALL_OK)
	{
		// check if the first block really contained the metadata
		if (FLAC__stream_decoder_process_until_end_of_metadata(decoder) == FALSE)	
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_BAD_HEADER;
		}
	}

	// libFLAC: check if FLAC file has 16 bit resolution, mp3 streams support only 16 bit resolution
	if (err == ALL_OK)
	{
		switch (ClientData.bps)
		{
		case 16:
			break;
		case 24:
		default:
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LAME_ONLY_16_BIT;
		}
	}

	// libFLAC: check if total_samples count is in the STREAMINFO
	if (err == ALL_OK)
	{
		if (ClientData.total_samples == 0)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_BAD_HEADER;
		}
	}

	// libmp3lame: initialize encoder
	if (err == ALL_OK)
	{
		lame_gfp = lame_init();
		if (lame_gfp == NULL)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LAME_INIT;
		}
	}

	// libFLAC: initialize the metadata reader
	// https://xiph.org/flac/api/group__flac__metadata__level2.html
	if (err == ALL_OK)
	{
		FLAC__StreamMetadata_VorbisComment_Entry commententry;
		FLAC__Metadata_Chain* FLACchain;
		FLAC__Metadata_Iterator* FLACchainIterator;
		FLAC__StreamMetadata* FLACMetaData;
		FLAC__IOCallbacks metadata_callbacks;
		FLAC__bool MetaDataOK;
		FLAC__MetadataType FLACMetaDataType;
		int vorbiscommentoffset;
		char* commentname, * commentvalue;

		for (int i = 0; i < MD_NUMBER; i++) MetaDataTrans[i].present = false; // clear the metadata transfer variables
		metadata_callbacks.read = read_iocallback;
		metadata_callbacks.write = write_iocallback;
		metadata_callbacks.tell = tell_iocallback;
		metadata_callbacks.eof = eof_iocallback;
		metadata_callbacks.seek = seek_iocallback;
		metadata_callbacks.close = close_iocallback;

		FLACchain = FLAC__metadata_chain_new();
		if (FLAC__metadata_chain_read_with_callbacks(FLACchain, ClientData.fin, metadata_callbacks) == FALSE)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_METADATA;
		}
		
		if (err == ALL_OK)
		{
			FLACchainIterator = FLAC__metadata_iterator_new();
			FLAC__metadata_iterator_init(FLACchainIterator, FLACchain);

			// go through the FLAC metadata blocks and find the "FLAC__METADATA_TYPE_VORBIS_COMMENT" block
			do
			{
				FLACMetaData = FLAC__metadata_iterator_get_block(FLACchainIterator);

				FLACMetaDataType = FLAC__metadata_iterator_get_block_type(FLACchainIterator);
				if (FLACMetaDataType == FLAC__METADATA_TYPE_VORBIS_COMMENT)
				{
					// copy the metadata to the transfer variables
					// https://xiph.org/flac/api/group__flac__metadata__object.html
					/*
					TITLE
						Track / Work name
					VERSION
						The version field may be used to differentiate multiple versions of the same track title in a single collection. (e.g.remix info)
					ALBUM
						The collection name to which this track belongs
					TRACKNUMBER
						The track number of this piece if part of a specific larger collection or album
					ARTIST
						The artist generally considered responsible for the work.In popular music this is usually the performing band or singer.For classical music it would be the composer.For an audio book it would be the author of the original text.
					PERFORMER
						The artist(s) who performed the work.In classical music this would be the conductor, orchestra, soloists.In an audio book it would be the actor who did the reading.In popular music this is typically the same as the ARTIST and is omitted.
					COPYRIGHT
						Copyright attribution, e.g., '2001 Nobody's Band' or '1999 Jack Moffitt'
					LICENSE
						License information, eg, 'All Rights Reserved', 'Any Use Permitted', a URL to a license such as a Creative Commons license("www.creativecommons.org/blahblah/license.html") or the EFF Open Audio License('distributed under the terms of the Open Audio License. see http://www.eff.org/IP/Open_licenses/eff_oal.html for details'), etc.
					ORGANIZATION
						Name of the organization producing the track(i.e.the 'record label')
					DESCRIPTION
						A short text description of the contents
					GENRE
						A short text indication of music genre
					DATE
						Date the track was recorded
					LOCATION
						Location where track was recorded
					CONTACT
						Contact information for the creators or distributors of the track.This could be a URL, an email address, the physical address of the producing label.
					ISRC
						ISRC number for the track; see the ISRC intro page for more information on ISRC numbers.
					*/
					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "ALBUM");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_ALBUM].text = new char[MAXMETADATA];
							MetaDataTrans[MD_ALBUM].present = true;
							strcpy_s(MetaDataTrans[MD_ALBUM].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "ARTIST");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_ARTIST].text = new char[MAXMETADATA];
							MetaDataTrans[MD_ARTIST].present = true;
							strcpy_s(MetaDataTrans[MD_ARTIST].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "DATE");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_DATE].text = new char[MAXMETADATA];
							MetaDataTrans[MD_DATE].present = true;
							strcpy_s(MetaDataTrans[MD_DATE].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "DISCNUMBER");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_DISCNUMBER].text = new char[MAXMETADATA];
							MetaDataTrans[MD_DISCNUMBER].present = true;
							strcpy_s(MetaDataTrans[MD_DISCNUMBER].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "GENRE");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_GENRE].text = new char[MAXMETADATA];
							MetaDataTrans[MD_GENRE].present = true;
							strcpy_s(MetaDataTrans[MD_GENRE].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "TITLE");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_TITLE].text = new char[MAXMETADATA];
							MetaDataTrans[MD_TITLE].present = true;
							strcpy_s(MetaDataTrans[MD_TITLE].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "TRACKNUMBER");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_TRACKNUMBER].text = new char[MAXMETADATA];
							MetaDataTrans[MD_TRACKNUMBER].present = true;
							strcpy_s(MetaDataTrans[MD_TRACKNUMBER].text, MAXMETADATA, commentvalue);
						}
					}
				}

				MetaDataOK = FLAC__metadata_iterator_next(FLACchainIterator);
			} while (MetaDataOK == TRUE);

			FLAC__metadata_chain_delete(FLACchain);
			if (FLAC__stream_decoder_reset(decoder) != TRUE) // reset the FLAC decoder because the metadata reader has changed the file pointer and not the correct audio stream will be read for the decoder
			{
				fclose(fin);
				FLAC__stream_decoder_delete(decoder);
				myparams->ThreadInUse = false;
				ExitEncThread(FAIL_LIBFLAC_ALLOC, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
			}

			FLAC__stream_decoder_process_until_end_of_metadata(decoder);	// go back to the exact position where we were before the metadata reading, otherwise there will be an additional pop sound at the beginning of the converted stream
		}
	}

	// libmp3lame: set encoder parameters
	if (err == ALL_OK)
	{
		switch (ClientData.channels)
		{
		case 1:
			lame_set_mode(lame_gfp, MONO);
			break;
		case 2:
			lame_set_mode(lame_gfp, JOINT_STEREO);
			break;
		default:
			// only mono and stereo streams are supported by libmp3lame
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LAME_MAX_2_CHANNEL;
			break;
		}
	}
	if (err == ALL_OK)
	{	
		// Internal algorithm selection. True quality is determined by the bitrate but this variable will effect quality by selecting expensive or cheap algorithms.
		// quality=0..9.  0=best (very slow).  9=worst.
		// recommended:  2     near-best quality, not too slow
		// 5     good quality, fast
		// 7     ok quality, really fast
		lame_set_quality(lame_gfp, EncSettings.LAME_InternalEncodingQuality);

		// turn off automatic writing of ID3 tag data into mp3 stream we have to call it before 'lame_init_params', because that function would spit out ID3v2 tag data.
		lame_set_write_id3tag_automatic(lame_gfp, 0);

		// set libmp3lame encoder parameters for CBR encoding
		lame_set_num_channels(lame_gfp, ClientData.channels);
		lame_set_in_samplerate(lame_gfp, ClientData.sample_rate);
		lame_set_brate(lame_gfp, LAME_CBRBITRATES[EncSettings.LAME_CBRBitrate]);	// load bitrate setting from LUT

		// set libmp3lame encoder parameters for VBR encoding
		switch (EncSettings.LAME_EncodingMode)
		{
		case 0:	// CBR
			lame_set_VBR(lame_gfp, vbr_off);
			break;
		case 1:	// VBR
			lame_set_VBR(lame_gfp, vbr_mtrh);
			break;
		}

		lame_set_VBR_q(lame_gfp, EncSettings.LAME_VBRQuality); // VBR quality level.  0=highest  9=lowest

		// now that all the options are set, libmp3lame needs to analyze them and set some more internal options and check for problems
		if (lame_init_params(lame_gfp) != 0)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LAME_INIT;
		}
	}

	// libmp3lame: open the output file
	if (err == ALL_OK)
	{
		WCHAR* OutFileName;

		OutFileName = new WCHAR[MAXFILENAMELENGTH];
		wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH) - 4);	// leave out the "flac" from the end
		wcscat_s(OutFileName, MAXFILENAMELENGTH, L"mp3");
		if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			lame_close(lame_gfp);
			err = FAIL_FILE_OPEN;
		}
		delete[]OutFileName;
	}
			
	// libFLAC / libmp3lame: move the tags from the FLAC stream to the MP3 stream
	if (err == ALL_OK)
	{
		size_t  id3v2_size;
		unsigned char* id3v2tag;
		int imp3, owrite;

		// switch on ID3 v2 tags in the MP3 stream
		id3tag_init(lame_gfp);
		id3tag_v2_only(lame_gfp);

		// add the tags
		if (MetaDataTrans[MD_ALBUM].present == true) id3tag_set_album(lame_gfp, MetaDataTrans[MD_ALBUM].text);
		if (MetaDataTrans[MD_ARTIST].present == true) id3tag_set_artist(lame_gfp, MetaDataTrans[MD_ARTIST].text);
		if (MetaDataTrans[MD_DATE].present == true) id3tag_set_year(lame_gfp, MetaDataTrans[MD_DATE].text);
		if (MetaDataTrans[MD_GENRE].present == true) id3tag_set_genre(lame_gfp, MetaDataTrans[MD_GENRE].text);
		if (MetaDataTrans[MD_TITLE].present == true) id3tag_set_title(lame_gfp, MetaDataTrans[MD_TITLE].text);
		if (MetaDataTrans[MD_TRACKNUMBER].present == true) id3tag_set_track(lame_gfp, MetaDataTrans[MD_TRACKNUMBER].text);
		// http://id3.org/id3v2.3.0#Text_information_frames

		id3v2_size = lame_get_id3v2_tag(lame_gfp, 0, 0);
		id3v2tag = new unsigned char[id3v2_size];
		if (id3v2tag != 0)
		{
			imp3 = lame_get_id3v2_tag(lame_gfp, id3v2tag, id3v2_size);
			owrite = (int)fwrite(id3v2tag, 1, imp3, fout);
			if (owrite != imp3)
			{
				fclose(fin);
				fclose(fout);
				FLAC__stream_decoder_delete(decoder);
				lame_close(lame_gfp);
				err = FAIL_LAME_ID3TAG;
			}
			if (LAME_FLUSH == true) fflush(fout);
			delete[]id3v2tag;
		}

		for (int i = 0; i < MD_NUMBER; i++) if (MetaDataTrans[i].present == true) delete[] MetaDataTrans[i].text;	// free up the metadata transfer block
	}

	// libFLAC / libmp3lame: start the transcoding
	if (err == ALL_OK)
	{
		int imp3, owrite;
		BYTE *buffer_mp3, *buffer_raw;
		FLAC__bool ok = TRUE;
		FLAC__StreamDecoderState state;

		// allocate memory buffers
		buffer_raw = new BYTE[SIZE_RAW_BUFFER];
		buffer_mp3 = new BYTE[LAME_MAXMP3BUFFER];
		ClientData.buffer_out = buffer_raw;

		// set up the progress bar boundaries, block size is around 4k depending on resolution
		SendMessage(myparams->progress, PBM_SETPOS, 0, 0);
		SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, ClientData.total_samples / ClientData.blocksize));	

		// loop the libFLAC decoder until it reaches the end of the input file or returns an error
		do
		{
			ok = FLAC__stream_decoder_process_single(decoder);
			state = FLAC__stream_decoder_get_state(decoder);

			// feed the block of samples to the encoder
			switch (ClientData.channels)
			{
			case 2:
				imp3 = lame_encode_buffer_interleaved(lame_gfp, (short int*)buffer_raw, ClientData.blocksize, buffer_mp3, LAME_MAXMP3BUFFER);
				break;
			case 1:	// the interleaved version corrupts the mono stream
				imp3 = lame_encode_buffer(lame_gfp, (short int*)buffer_raw, NULL, ClientData.blocksize, buffer_mp3, LAME_MAXMP3BUFFER);
				break;
			}

			// was our output buffer big enough?
			if (imp3 < 0) ok = false;
			else
			{
				owrite = (int)fwrite(buffer_mp3, 1, imp3, fout);
				if (owrite != imp3) ok = false;
				if (LAME_FLUSH == true) fflush(fout);
			}

			SendMessage(myparams->progress, PBM_DELTAPOS, 1, 0);	// increase the progress bar
		} while ((state != FLAC__STREAM_DECODER_END_OF_STREAM && FLAC__STREAM_DECODER_SEEK_ERROR && FLAC__STREAM_DECODER_ABORTED && FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) && ok == TRUE);


		// may return one more mp3 frame
		if (ok)
		{
			if (LAME_NOGAP == true) imp3 = lame_encode_flush_nogap(lame_gfp, buffer_mp3, LAME_MAXMP3BUFFER);
			else imp3 = lame_encode_flush(lame_gfp, buffer_mp3, LAME_MAXMP3BUFFER);
			if (imp3 < 0) ok = false;
		}

		if (ok)
		{
			owrite = (int)fwrite(buffer_mp3, 1, imp3, fout);
			if (owrite != imp3) ok = false;
		}

		if (LAME_FLUSH == true && ok == TRUE) fflush(fout);

		delete[]buffer_raw;
		delete[]buffer_mp3;

		if (ok == false)
		{
			fclose(fin);
			fclose(fout);
			FLAC__stream_decoder_delete(decoder);
			lame_close(lame_gfp);
			err = FAIL_LAME_ENCODE;
		}
	}

	// libFLAC: close the decoder
	if (err == ALL_OK)
	{
		switch (FLAC__stream_decoder_get_state(decoder))
		{
		case FLAC__STREAM_DECODER_SEEK_ERROR:
		case FLAC__STREAM_DECODER_ABORTED:
		case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
			fclose(fin);
			fclose(fout);
			FLAC__stream_decoder_delete(decoder);
			lame_close(lame_gfp);
			err = FAIL_LIBFLAC_ENCODE;
			break;
		default:
			break;
		}

	}
	if (err == ALL_OK)
	{	
		if (FLAC__stream_decoder_finish(decoder) == FALSE) err = WARN_LIBFLAC_MD5;
		fclose(fout);
		fclose(fin);
		FLAC__stream_decoder_delete(decoder);
	}

	// libmp3lame: close the encoder
	if (err == ALL_OK)
	{
		if (lame_close(lame_gfp) != 0) err = FAIL_LAME_CLOSE;
		fclose(fin);
		fclose(fout);
	}
	
	myparams->ThreadInUse = false;
	ExitEncThread(err, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	return ALL_OK;
}