// Callback functions for libFLAC
#include "stdafx.h"
#include "helper_encode.h"

//
//	FUNCTION:	seek_callback_2FLAC(const FLAC__StreamEncoder, FLAC__uint64, void)
//
//	PURPOSE:	Provide function for libFLAC encoder data seeking
//
FLAC__StreamEncoderSeekStatus seek_callback_2FLAC(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	UNREFERENCED_PARAMETER(encoder);
	
	FILE *file = ((sClientData*)client_data)->fout;

	if(file == stdin) return FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED;
	else
		if(_fseeki64(file, absolute_byte_offset, SEEK_SET) < 0) return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
		else return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

//
//	FUNCTION:	tell_callback_2FLAC(const FLAC__StreamEncoder, FLAC__uint64, void)
//
//	PURPOSE:	Provide function for libFLAC encoder data seeking
//
FLAC__StreamEncoderTellStatus tell_callback_2FLAC(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	UNREFERENCED_PARAMETER(encoder);
	
	FILE *file = ((sClientData*)client_data)->fout;
	long long pos;

	if(file == stdin) return FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED;
	else
		if((pos = _ftelli64(file)) < 0) return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
		else
		{
			*absolute_byte_offset = (FLAC__uint64)pos;
			return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
		}
}

//
//	FUNCTION:	metadata_callback_2FLAC(const FLAC__StreamEncoder, const FLAC__StreamMetadata, void)
//
//	PURPOSE:	Provide function for libFLAC encoder metadata writing
//
void metadata_callback_2FLAC(const FLAC__StreamEncoder *encoder, const FLAC__StreamMetadata *metadata, void *client_data)
{	
	UNREFERENCED_PARAMETER(encoder);
	UNREFERENCED_PARAMETER(metadata);
	UNREFERENCED_PARAMETER(client_data);
}

//
//	FUNCTION:	write_callback_2WAV(const FLAC__StreamDecoder*, const FLAC__Frame*, const FLAC__int32* const, void)
//
//	PURPOSE:	Provide function for libFLAC decoder data writing
//
FLAC__StreamDecoderWriteStatus write_callback_2WAV(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	UNREFERENCED_PARAMETER(decoder);
	UNREFERENCED_PARAMETER(frame);
	
	FILE *f = ((sClientData*)client_data)->fout;
	const FLAC__uint32 total_size = (FLAC__uint32)(((sClientData*)client_data)->total_samples * ((sClientData*)client_data)->channels * (((sClientData*)client_data)->bps/8));
	size_t i;
	unsigned int chan, pos;
	FLAC__int32 temp;
	FLAC__byte* pcm;

	// write WAVE header before writing the first frame
	if(frame->header.number.sample_number == 0)
	{
		sWAVEheader WAVEheader;
		sFMTheader FMTheader;
		sDATAheader DATAheader;

		// fill up the WAVE file headers
		WAVEheader.ChunkID[0] = 'R';
		WAVEheader.ChunkID[1] = 'I';
		WAVEheader.ChunkID[2] = 'F';
		WAVEheader.ChunkID[3] = 'F';
		WAVEheader.ChunkSize = 36 + total_size;	// This is the size of the entire file in bytes minus 8 bytes for the two fields not included in this count: ChunkID and ChunkSize.
		WAVEheader.Format[0] = 'W';
		WAVEheader.Format[1] = 'A';
		WAVEheader.Format[2] = 'V';
		WAVEheader.Format[3] = 'E';
		FMTheader.ChunkID[0] = 'f';
		FMTheader.ChunkID[1] = 'm';
		FMTheader.ChunkID[2] = 't';
		FMTheader.ChunkID[3] = ' ';
		FMTheader.ChunkSize = 16;	// size of the format subchunk
		FMTheader.AudioFormat = 1;	// we are using PCM format
		FMTheader.NumChannels = ((sClientData*)client_data)->channels;
		FMTheader.SampleRate = ((sClientData*)client_data)->sample_rate;
		FMTheader.ByteRate = ((sClientData*)client_data)->sample_rate * ((sClientData*)client_data)->channels * (((sClientData*)client_data)->bps / 8);	// SampleRate * NumChannels * BitsPerSample/8
		FMTheader.BlockAlign = ((sClientData*)client_data)->channels * (((sClientData*)client_data)->bps / 8);	// NumChannels * BitsPerSample/8
		FMTheader.BitsPerSample = ((sClientData*)client_data)->bps;
		DATAheader.ChunkID[0] = 'd';
		DATAheader.ChunkID[1] = 'a';
		DATAheader.ChunkID[2] = 't';
		DATAheader.ChunkID[3] = 'a';
		DATAheader.ChunkSize = total_size;

		i = fwrite(&WAVEheader, 1, 12, f);
		if (i != 12) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;	// write error
		i = fwrite(&FMTheader, 1, 24, f);
		if (i != 24) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		i = fwrite(&DATAheader, 1, 8, f);
		if (i != 8) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	// write decoded PCM samples
	switch (((sClientData*)client_data)->bps)
	{
		case 16:	pcm = new FLAC__byte[frame->header.blocksize * ((sClientData*)client_data)->channels *2];
					for(chan=0; chan<((sClientData*)client_data)->channels; chan++)
					{
						for(pos=0; pos<frame->header.blocksize; pos++)
						{
							temp = buffer[chan][pos];
							pcm[((sClientData*)client_data)->channels*pos*2 + chan*2] = (FLAC__byte)temp;
							temp = temp >> 8;
							pcm[((sClientData*)client_data)->channels*pos*2 + chan*2 +1] = (FLAC__byte)temp;
						}
					}
					i = fwrite(pcm, ((sClientData*)client_data)->channels*2, frame->header.blocksize, f);
					delete []pcm;
					if(i!=frame->header.blocksize) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
					break;
		
		case 24:	pcm = new FLAC__byte[frame->header.blocksize * ((sClientData*)client_data)->channels *3];
					for(chan=0; chan<((sClientData*)client_data)->channels; chan++)
					{
						for(pos=0; pos<frame->header.blocksize; pos++)
						{
							temp = buffer[chan][pos];
							pcm[((sClientData*)client_data)->channels*pos*3 + chan*3] = (FLAC__byte)temp;
							temp = temp >> 8;
							pcm[((sClientData*)client_data)->channels*pos*3 + chan*3 +1] = (FLAC__byte)temp;
							temp = temp >> 8;
							pcm[((sClientData*)client_data)->channels*pos*3 + chan*3 +2] = (FLAC__byte)temp;
						}
					}
					i = fwrite(pcm, ((sClientData*)client_data)->channels*3, frame->header.blocksize, f);
					delete []pcm;
					if(i!=frame->header.blocksize) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
					break;
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

//
//	FUNCTION:	read_callback_2WAV(const FLAC__StreamDecoder*, FLAC__byte[], size_t*, void*)
//
//	PURPOSE:	Provide function for libFLAC decoder data reading
//
FLAC__StreamDecoderReadStatus read_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	UNREFERENCED_PARAMETER(decoder);

	if(*bytes > 0)
	{
		*bytes = fread(buffer, sizeof(FLAC__byte), *bytes, ((sClientData*)client_data)->fin);
		if(ferror(((sClientData*)client_data)->fin))
			return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
		else if(*bytes == 0)
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		else
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	}
	else
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT; // abort to avoid a deadlock
}

//
//	FUNCTION:	seek_callback_2WAV(const FLAC__StreamDecoder*, FLAC__uint64, void)
//
//	PURPOSE:	Provide function for libFLAC decoder data reading
//
FLAC__StreamDecoderSeekStatus seek_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	UNREFERENCED_PARAMETER(decoder);

	if(((sClientData*)client_data)->fin == stdin)
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
	else if(_fseeki64(((sClientData*)client_data)->fin, (off_t)absolute_byte_offset, SEEK_SET) < 0)
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	else
		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

//
//	FUNCTION:	tell_callback_2WAV(const FLAC__StreamDecoder*, FLAC__uint64*, void)
//
//	PURPOSE:	Provide function for libFLAC decoder file position reading
//
FLAC__StreamDecoderTellStatus tell_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	UNREFERENCED_PARAMETER(decoder);

	long long pos;

	if(((sClientData*)client_data)->fin == stdin)
		return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
	else if((pos = _ftelli64(((sClientData*)client_data)->fin)) < 0)
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
	else {
		*absolute_byte_offset = (FLAC__uint64)pos;
		return FLAC__STREAM_DECODER_TELL_STATUS_OK;
	}
}

//
//	FUNCTION:	length_callback_2WAV(const FLAC__StreamDecoder*, FLAC__uint64*, void)
//
//	PURPOSE:	Provide function for libFLAC decoder filesize reading
//
FLAC__StreamDecoderLengthStatus length_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	UNREFERENCED_PARAMETER(decoder);

	struct __stat64 filestats;

	if(((sClientData*)client_data)->fin == stdin)
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
	else if(_fstat64(_fileno(((sClientData*)client_data)->fin), &filestats) != 0)
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	else {
		*stream_length = (FLAC__uint64)filestats.st_size;
		return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
	}
}

//
//	FUNCTION:	eof_callback_2WAV(const FLAC__StreamDecoder*, void)
//
//	PURPOSE:	Provide function for libFLAC decoder end of file reading
//
FLAC__bool eof_callback_2WAV(const FLAC__StreamDecoder *decoder, void *client_data)
{
	UNREFERENCED_PARAMETER(decoder);

	return feof(((sClientData*)client_data)->fin)? true : false;
} 

//
//	FUNCTION:	metadata_callback_2WAV(const FLAC__StreamDecoder*, const FLAC__StreamMetadata*, void)
//
//	PURPOSE:	Provide function for libFLAC decoder metadata writing
//
void metadata_callback_2WAV(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	UNREFERENCED_PARAMETER(decoder);

	// save WAV metadata for other callback processes
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{
		((sClientData*)client_data)->total_samples = metadata->data.stream_info.total_samples;
		((sClientData*)client_data)->sample_rate = metadata->data.stream_info.sample_rate;
		((sClientData*)client_data)->channels = metadata->data.stream_info.channels;
		((sClientData*)client_data)->bps = metadata->data.stream_info.bits_per_sample;
		((sClientData*)client_data)->blocksize = metadata->data.stream_info.max_blocksize;
	}
}

//
//	FUNCTION:	error_callback_2WAV(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus, void)
//
//	PURPOSE:	Provide function for libFLAC decoder error message handling
//
void error_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	UNREFERENCED_PARAMETER(decoder);
	UNREFERENCED_PARAMETER(client_data);

	fprintf(stderr, "Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}