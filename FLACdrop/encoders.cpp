#include "stdafx.h"
#include "FLACdrop.h"
#include "Encoders.h"
#include "lame.h"

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
	static HANDLE aThread[MAX_THREADS];
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
	SendMessage(myparams->text, WM_SETTEXT, 0, (LPARAM)L"Encoding is in progress");

	ghSemaphore = CreateSemaphore(NULL, EncSettings.OUT_Threads-1, MAX_THREADS, NULL);				// number of the semaphore is the number of threads we would like to use in parallel
	
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
			switch (EncSettings.OUT_Type)
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
			tID = SearchFreeThread(EncParams);
			EncParams[tID].ThreadInUse = true;
			EncParams[tID].progress = myparams->progress[tID];
			wcscpy_s(EncParams[tID].filename, MAXFILENAMELENGTH, Filename);
			aThread[tID] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&Encode_FLAC2WAV, &EncParams[tID], 0, NULL);
			ThreadStarted = true;
		}
		
		if (ThreadStarted == true) WaitForSingleObject(ghSemaphore, INFINITE);	// a thread was started so we have to decrease the count of the semaphore with one, continue only if at least one thread is free
		else SendMessage(myparams->progresstotal, PBM_DELTAPOS, 1, 0);			// no thread was started (file was not recognized), but we have to increase the total progress bar
	}

	WaitForMultipleObjects(EncSettings.OUT_Threads-1, aThread, TRUE, INFINITE);	// wait for all threads to terminate

	// release the memory which the system allocated for the file name transfer
	DragFinish(myparams->filedrop);
	myparams->EncoderInUse = false;
		
	CloseHandle(ghSemaphore);
	for (int i = 0; i<EncSettings.OUT_Threads; i++) CloseHandle(aThread[i]);
	
	SendMessage(myparams->text, WM_SETTEXT, 0, (LPARAM)L"Waiting for audio files to be dropped...");

	return ALL_OK;		// only to prevent compiler warning message
}

//
//	FUNCTION:	ExitEncThread(int, HANDLE, HWND)
//
//	PURPOSE:	Exits the encoder thread, releases the semaphore and displays the appropiate text on the main window
//
void ExitEncThread(int ExitCode, HANDLE Semaphore, HWND progresstotal, WCHAR *filename, int type)
{
	WCHAR rn[] = L"\r\n";
	WCHAR FLAC[] = L"FLAC: ";
	WCHAR MP3[] = L"MP3: ";
	WCHAR WAV[] = L"WAV: ";

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

	SendMessage(progresstotal, PBM_DELTAPOS, 1, 0);						// increase the total progress bar
	ReleaseSemaphore(Semaphore, 1, NULL);
	ExitThread(ExitCode);
}

//
//	FUNCTION:	Encode_WAV2MP3(sEncodingParameters* )
//
//	PURPOSE:	Encode WAV stream to MP3 stream
//
DWORD WINAPI Encode_WAV2MP3(LPVOID *params)
{
	sEncodingParameters *myparams = (sEncodingParameters*)params;
	
	lame_global_flags *lame_gfp;
	BYTE *buffer_mp3, *buffer_wav;
	int imp3, owrite;
	FILE *fin, *fout;
	WCHAR* OutFileName;
	bool ok = true;

	unsigned int total_samples = 0;	// can use a 32-bit number due to WAV file size limitation
	sWAVEheader WAVEheader;
	sFMTheader FMTheader;
	sDATAheader DATAheader;

	// setup the message area
	SendMessage(myparams->progress, PBM_SETPOS, 0, 0);	// reset the progress bar

	if ((_wfopen_s(&fin, myparams->filename, L"rb")) != NULL)
	{
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_FILE_OPEN, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}

	// read wav header and check it
	if (fread(&WAVEheader, 1, 12, fin) != 12)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_FILE_OPEN, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}
	if (memcmp(WAVEheader.ChunkID, "RIFF", 4) || memcmp(WAVEheader.Format, "WAVE", 4))
	{
		fclose(fin);
		ExitEncThread(FAIL_WAV_BAD_HEADER, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}

	// read the format chunk's header only for its chunk size
	if (fread(&DATAheader, 1, 8, fin) != 8)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_WAV_BAD_HEADER, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}
	fseek(fin, -8, SEEK_CUR);
	// read the complete wave file header according to its actual chunk size (16, 18 or 40 byte)
	if (fread(&FMTheader, 1, DATAheader.ChunkSize + 8, fin) != DATAheader.ChunkSize + 8)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_WAV_BAD_HEADER, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}

	// check if the wav file has PCM uncompressed data
	if (FMTheader.AudioFormat != 1)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_WAV_UNSUPPORTED, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}

	// check if the WAVE file has 16 or 24 bit resolution
	switch (FMTheader.BitsPerSample)
	{
	case 16:
	case 24:
		break;
	default:
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_INPUT_NOT_16_24_BIT, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}

	// search for the data chunk
	do
	{
		fread(&DATAheader, 1, 8, fin);
		fseek(fin, DATAheader.ChunkSize, SEEK_CUR);
	} while (memcmp(DATAheader.ChunkID, "data", 4));
	fseek(fin, -DATAheader.ChunkSize, SEEK_CUR);													// go back to the beginning of the data chunk
	total_samples = DATAheader.ChunkSize / FMTheader.NumChannels / (FMTheader.BitsPerSample / 8);	// sound data's size divided by one sample's size
	SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, total_samples / READSIZE_MP3));		// set up the progress bar boundaries

	// initialize lame encoder
	lame_gfp = lame_init();
	if (lame_gfp == NULL)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_LAME_INIT, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}

	switch (FMTheader.NumChannels)
	{
		case 1:
			lame_set_mode(lame_gfp, MONO);
			break;
		case 2:
			lame_set_mode(lame_gfp, JOINT_STEREO);
			break;
	}

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
	lame_set_brate(lame_gfp, LAME_CBRBITRATES[EncSettings.LAME_CBRBitrate]);	// load bitrate setting from LUT

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
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_LAME_INIT, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}
	
	// open output file
	OutFileName = new WCHAR[MAXFILENAMELENGTH];
	wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH)-3);	// leave out the "wav" from the end
	wcscat_s(OutFileName, MAXFILENAMELENGTH, L"mp3");
	if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_FILE_OPEN, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}
	delete []OutFileName;
	
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

	size_t left, need;

	left = (size_t)total_samples;
	// read blocks of samples from WAVE file and feed to the encoder
	while(ok && left)
	{
		need = (left>READSIZE_MP3? (size_t)READSIZE_MP3 : (size_t)left);	// calculate the number of samples to read
		if (fread(buffer_wav, FMTheader.NumChannels * (FMTheader.BitsPerSample/8), need, fin) != need)
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
				owrite = (int) fwrite(buffer_mp3, 1, imp3, fout);
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
		owrite = (int) fwrite(buffer_mp3, 1, imp3, fout);
		if (owrite != imp3) ok = false;
	}

	if (LAME_FLUSH == true && ok == true) fflush(fout);

	delete []buffer_wav;
	delete []buffer_mp3;
	fclose(fin);
	fclose(fout);
	myparams->ThreadInUse = false;

	if (ok == true)
	{
		if (lame_close(lame_gfp) == 0) ExitEncThread(ALL_OK, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
		else ExitEncThread(FAIL_LAME_CLOSE, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	}
	else ExitEncThread(FAIL_LAME_ENCODE, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	
	return ALL_OK;	// only to prevent compiler warning message
}

//
//	FUNCTION:	Encode_WAV2FLAC(sEncodingParameters* )
//
//	PURPOSE:	Encode WAV stream to FLAC stream
//
DWORD WINAPI Encode_WAV2FLAC(LPVOID *params)
{
	sEncodingParameters *myparams = (sEncodingParameters*)params;

	FLAC__bool ok = TRUE;
	FLAC__StreamEncoder *encoder = NULL;
	FLAC__StreamEncoderInitStatus init_status;
	//FLAC__StreamMetadata *metadata[2];
	//FLAC__StreamMetadata_VorbisComment_Entry entry;
	FLAC__byte *buffer_wav;				// READSIZE * bytes per sample * channels, we read the WAVE data into here
	FLAC__int32 *buffer_flac;			// READSIZE * channels
	unsigned int total_samples = 0;		// can use a 32-bit number due to WAV file size limitations in specification

	sWAVEheader WAVEheader;
	sFMTheader FMTheader;
	sDATAheader DATAheader;
	sClientData ClientData;

	FILE *fin, *fout;
	WCHAR* OutFileName;
	
	// setup the message area
	SendMessage(myparams->progress, PBM_SETPOS, 0, 0);	// reset the progress bar
	
	if((_wfopen_s(&fin, myparams->filename, L"rb")) != NULL)
	{
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_FILE_OPEN, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	}
	
	// read wav header and check it
	if(fread(&WAVEheader, 1, 12, fin) != 12)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_FILE_OPEN, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	}
	if(memcmp(WAVEheader.ChunkID, "RIFF", 4) || memcmp(WAVEheader.Format, "WAVE", 4))
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_WAV_BAD_HEADER, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	}

	// read the format chunk's header only to get its chunk size
	if(fread(&DATAheader, 1, 8, fin) != 8)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_WAV_BAD_HEADER, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	}
	fseek(fin, -8, SEEK_CUR);
	// read the complete wave file header according to its actual chunk size (16, 18 or 40 byte)
	if(fread(&FMTheader, 1, DATAheader.ChunkSize+8, fin) != DATAheader.ChunkSize+8)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_WAV_BAD_HEADER, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	}

	// check if the wav file has PCM uncompressed data
	if(FMTheader.AudioFormat != 1)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_WAV_UNSUPPORTED, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	}

	// check if the WAVE file has 16 or 24 bit resolution
	switch(FMTheader.BitsPerSample)
	{
		case 16:
		case 24:
			break;
		default:
			fclose(fin);
			myparams->ThreadInUse = false;
			ExitEncThread(FAIL_INPUT_NOT_16_24_BIT, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	}

	// search for the data chunk
	do
	{
		fread(&DATAheader, 1, 8, fin);
		fseek(fin, DATAheader.ChunkSize, SEEK_CUR);
	}
	while (memcmp(DATAheader.ChunkID, "data", 4) );
	fseek(fin, -DATAheader.ChunkSize, SEEK_CUR);													// go back to the beginning of the data chunk
	total_samples = DATAheader.ChunkSize / FMTheader.NumChannels / (FMTheader.BitsPerSample / 8);	// sound data's size divided by one sample's size
	SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, total_samples/READSIZE_FLAC));			// set up the progress bar boundaries
   
	// allocate the libFLAC encoder and data buffers
	if((encoder = FLAC__stream_encoder_new()) == NULL)
	{
		fclose(fin);
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_LIBFLAC_ALLOC, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	}
	buffer_wav = new FLAC__byte[READSIZE_FLAC * FMTheader.NumChannels * (FMTheader.BitsPerSample / 8)];
	buffer_flac = new FLAC__int32[READSIZE_FLAC * FMTheader.NumChannels];

	// set the encoder parameters
	ok &= FLAC__stream_encoder_set_verify(encoder, EncSettings.FLAC_Verify);
	ok &= FLAC__stream_encoder_set_compression_level(encoder, EncSettings.FLAC_EncodingQuality);
	ok &= FLAC__stream_encoder_set_channels(encoder, FMTheader.NumChannels);
	ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, FMTheader.BitsPerSample);
	ok &= FLAC__stream_encoder_set_sample_rate(encoder, FMTheader.SampleRate);
	ok &= FLAC__stream_encoder_set_total_samples_estimate(encoder, total_samples);

	// now add some metadata; we'll add some tags and a padding block
//	if(ok)
//	{
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
//		}

//		metadata[1]->length = 1234;	// set the padding length

//		ok = FLAC__stream_encoder_set_metadata(encoder, metadata, 2);
//	}

	// initialize encoder
	if(ok)
	{
		OutFileName = new WCHAR[MAXFILENAMELENGTH];
		wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH)-3);	// leave out the "wav" from the end
		wcscat_s(OutFileName, MAXFILENAMELENGTH, L"flac");
		if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
		{
			fclose(fin);
			myparams->ThreadInUse = false;
			ExitEncThread(FAIL_FILE_OPEN, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
		}
		ClientData.fout = fout;
		init_status = FLAC__stream_encoder_init_stream(encoder, write_callback_2FLAC, seek_callback_2FLAC, tell_callback_2FLAC, metadata_callback_2FLAC, &ClientData);
		delete []OutFileName;

		if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
		{
			ok = false;
		}
	}

	// read blocks of samples from WAVE file and feed to the encoder
	size_t left, need;
	if(ok)
	{
		left = (size_t)total_samples;
		while(ok && left)
		{
			need = (left>READSIZE_FLAC? (size_t)READSIZE_FLAC : (size_t)left);	// calculate the number of samples to read
			if(fread(buffer_wav, FMTheader.NumChannels * (FMTheader.BitsPerSample/8), need, fin) != need)
			{
				// error during reading from WAVE file
				ok = false;
			}
			else
			{
				// convert the packed little-endian PCM samples from WAVE file into an interleaved FLAC__int32 buffer for libFLAC
				size_t i;
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
	}

	delete []buffer_wav;
	delete []buffer_flac;

	ok &= FLAC__stream_encoder_finish(encoder);
	// now that encoding is finished, the metadata can be freed
	//FLAC__metadata_object_delete(metadata[0]);
	//FLAC__metadata_object_delete(metadata[1]);

	FLAC__stream_encoder_delete(encoder);
	fclose(fin);
	fclose(fout);
	myparams->ThreadInUse = false;

	if (ok == TRUE) ExitEncThread(ALL_OK, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	else ExitEncThread(FAIL_LIBFLAC_ENCODE, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	
	return ALL_OK;	// only to prevent compiler warning message
}

//
//	FUNCTION:	Encode_FLAC2WAV(sEncodingParameters* )
//
//	PURPOSE:	Encode FLAC stream to WAV stream
//
DWORD WINAPI Encode_FLAC2WAV(LPVOID *params)
{
	sEncodingParameters *myparams = (sEncodingParameters*)params;

	FLAC__bool ok = TRUE;
	FLAC__StreamDecoder *decoder = 0;
	FLAC__StreamDecoderInitStatus init_status;
	FLAC__StreamDecoderState state;
	
	FILE *fin, *fout;
	WCHAR* OutFileName;
	sClientData ClientData;

	// setup the message area
	SendMessage(myparams->progress, PBM_SETPOS, 0, 0);		// reset the progress bar

	// allocate the decoder
	if((decoder = FLAC__stream_decoder_new()) == NULL)
	{
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_LIBFLAC_ALLOC, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);
	}

	// set the decoder parameters
	ok &= FLAC__stream_decoder_set_md5_checking(decoder, EncSettings.FLAC_MD5check);

	// open the input file
	if ((_wfopen_s(&fin, myparams->filename, L"r+b")) != NULL)
	{
		myparams->ThreadInUse = false;
		ExitEncThread(FAIL_FILE_OPEN, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);
	}
	ClientData.fin = fin;

	// initialize decoder
	if(ok)
	{
		init_status = FLAC__stream_decoder_init_stream(decoder, read_callback_2WAV,seek_callback_2WAV, tell_callback_2WAV, length_callback_2WAV, eof_callback_2WAV, write_callback_2WAV, metadata_callback_2WAV, error_callback_2WAV, &ClientData);

		if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			ok = FALSE;
		}
	}

	if(ok)
	{
		// check the FLAC file parameters
		ok = FLAC__stream_decoder_process_until_end_of_metadata(decoder);	// read the first block which contains the metadata
		if (ok == FALSE)													// check if the first block really contained the metadata
		{
			fclose(fin);
			myparams->ThreadInUse = false;
			ExitEncThread(FAIL_LIBFLAC_BAD_HEADER, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);
		}
		switch(ClientData.bps)												// check if FLAC file has 16 or 24 bit resolution
		{
			case 16:
			case 24:
				break;
			default:
				fclose(fin);
				myparams->ThreadInUse = false;
				ExitEncThread(FAIL_INPUT_NOT_16_24_BIT, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);
		}
		if (ClientData.total_samples == 0)									// check if total_samples count is in the STREAMINFO
		{
			fclose(fin);
			myparams->ThreadInUse = false;
			ExitEncThread(FAIL_LIBFLAC_BAD_HEADER, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);
		}

		// start the decoding
		if(ok)
		{
			// open the output file
			OutFileName = new WCHAR[MAXFILENAMELENGTH];
			wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH)-4);	// leave out the "flac" from the end
			wcscat_s(OutFileName, MAXFILENAMELENGTH, L"wav");
			if((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
			{	
				delete []OutFileName;
				fclose(fin);
				myparams->ThreadInUse = false;
				ExitEncThread(FAIL_FILE_OPEN, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);
			}
			delete []OutFileName;
			ClientData.fout = fout;

			SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, ClientData.total_samples/ClientData.blocksize));	// set up the progress bar boundaries, block size is around 4k depending on resolution
			// loop the decoder until it reaches the end of the input file or returns an error
			do
			{
				ok = FLAC__stream_decoder_process_single(decoder);
				state = FLAC__stream_decoder_get_state(decoder);
				SendMessage(myparams->progress, PBM_DELTAPOS, 1, 0);	// increase the progress bar
			}
			while ((state != FLAC__STREAM_DECODER_END_OF_STREAM && FLAC__STREAM_DECODER_SEEK_ERROR && FLAC__STREAM_DECODER_ABORTED && FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) && ok == TRUE);
		}
	}
	
	switch(state)
	{
		case FLAC__STREAM_DECODER_SEEK_ERROR:
		case FLAC__STREAM_DECODER_ABORTED:
		case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
			ok = false;
			break;
		default:
			break;
	}
	ok &= FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);
	fclose(fout);
	fclose(fin);
	myparams->ThreadInUse = false;

	if (ok == TRUE) ExitEncThread(ALL_OK, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);
	else ExitEncThread(FAIL_LIBFLAC_DECODE, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);

	return ALL_OK;
}

//
//	FUNCTION:	write_callback_2FLAC(const FLAC__StreamEncoder, const FLAC__byte, size_t, unsigned, unsigned, void)
//
//	PURPOSE:	Provide function for libFLAC encoder data writing
//
FLAC__StreamEncoderWriteStatus write_callback_2FLAC(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data)
{
	UNREFERENCED_PARAMETER(encoder);
	UNREFERENCED_PARAMETER(samples);	
	UNREFERENCED_PARAMETER(current_frame);
	
	FILE *file = ((sClientData*)client_data)->fout;
	
	if(fwrite(buffer, sizeof(FLAC__byte), bytes, file) == bytes) return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
	else return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}