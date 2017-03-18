#include "stream_encoder.h"
#include "stream_decoder.h"
//#include "metadata.h"

#define READSIZE_FLAC 1048576		// size of a block to read from disk in bytes for the FLAC encoder algorithm
#define READSIZE_MP3 8192			// size of a block to read from disk in bytes for the MP3 encoder algorithm
#define MAXFILENAMELENGTH 1024		// maximum size of a file name with full path
#define EVENTLOGSIZE 65536			// maximum character size of the event log
#define OUT_TYPE_FLAC 0
#define OUT_TYPE_MP3 1
#define OUT_TYPE_WAV 2

// default values of system variables
#define FLAC_ENCODINGQUALITY 6	// 1..8
#define FLAC_MAXENCODINGQUALITY 8
#define FLAC_VERIFY false
#define FLAC_MD5CHECK true
#define LAME_FLUSH true
#define LAME_NOGAP false
#define LAME_CBRBITRATE 10
#define LAME_INTERNALQUALITY 2	// 0..9
#define LAME_MAXINTERNALQUALITY 9
#define LAME_VBRQUALITY 1		// 0..9
#define LAME_MAXVBRQUALITY 9
#define LAME_ENCTYPE 0			// 0: CBR; 1: VBR
#define OUT_TYPE 0
#define OUT_THREADS 1			// number of batch processing threads
#define MAX_THREADS 8			// maximum number of batch processing threads

// LAME encoding bitrates
const TCHAR LAME_CBRBITRATES_TEXT [][4] = {
	TEXT ("48"), TEXT("64"), TEXT("80"), TEXT("96"), TEXT("112"), TEXT("128"), TEXT("160"), TEXT("192"), TEXT("224"), TEXT("256"), TEXT("320")};
const int LAME_CBRBITRATES[] = {
	48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
#define LAME_CBRBITRATES_QUANTITY 10	// quantity of "LAME_BITRATES"-1

// failure codes
#define ALL_OK						0
#define FAIL_FILE_OPEN				1
#define FAIL_WAV_BAD_HEADER			2
#define FAIL_WAV_UNSUPPORTED		3
#define FAIL_INPUT_NOT_16_24_BIT	4
#define FAIL_LIBFLAC_BAD_HEADER		5
#define FAIL_LIBFLAC_ALLOC			6
#define FAIL_LIBFLAC_ENCODE			7
#define FAIL_LIBFLAC_DECODE			8
#define FAIL_REGISTRY_OPEN			9
#define FAIL_REGISTRY_WRITE			10
#define FAIL_REGISTRY_READ			11
#define FAIL_LAME_INIT				12
#define FAIL_LAME_ID3TAG			13
#define FAIL_LAME_ENCODE			14
#define FAIL_LAME_CLOSE				15

// failure messages for failure codes
const WCHAR ErrMessage [][50] = {
	L"OK\r\n",											//0
	L"Error during opening the file\r\n",				//1
	L"Invalid WAVE file header\r\n",					//2
	L"Unsupported WAVE file compression format\r\n",	//3
	L"Only 16 and 24 bit files are supported\r\n",		//4
	L"Invalid FLAC file header\r\n",					//5
	L"Error during allocating libFLAC encoder\r\n",		//6
	L"Encoding failed\r\n",								//7
	L"Decoding failed\r\n",								//8
	L"Registry open failed\r\n",						//9
	L"Registry writing failed\r\n",						//10
	L"Registry reading failed\r\n",						//11
	L"Error during libmp3lame initialization\r\n",		//12
	L"Error during libmp3lame writing ID3TAG\r\n",		//13
	L"Error during libmp3lame encoding\r\n",			//14
	L"Error during libmp3lame closing\r\n"};			//15

// global encoder settings
struct sEncoderSettings
{
	bool FLAC_Verify;
	bool FLAC_MD5check;
	int FLAC_EncodingQuality;			// 1..8
	int LAME_CBRBitrate;				// 0..LAME_CBRBITRATES_QUANTITY
	int LAME_VBRQuality;				// 0..9
	int LAME_InternalEncodingQuality;	// 0..9
	int LAME_EncodingMode;				// 0: CBR; 1: VBR
	bool LAME_Flush;
	bool LAME_NoGap;
	int OUT_Type;						// 0: FLAC; 1: MP3
	int OUT_Threads;					// 1..MAX_THREADS
};

// wave file header
struct sWAVEheader
{
	char ChunkID[4];		// "RIFF"
	int ChunkSize;			// size of the complete WAVE file
	char Format[4];			// "WAVE"
};

// wave file format chunk header
struct sFMTheader
{
	char ChunkID[4];		// "fmt "
	int ChunkSize;			// 16, 18 or 40
	short AudioFormat;
	short NumChannels;
	int SampleRate;
	int ByteRate;			// SampleRate * NumChannels * BitsPerSample/8
	short BlockAlign;		// NumChannels * BitsPerSample/8
	short BitsPerSample;
	short ExtensionSize;	// Size of the extension (0 or 22)
	short ValidBitsPerSample;
	int ChannelMask;		// Speaker position mask
	char SubFormat[16];		// GUID, including the data format code
};

// wave file data chunk header
struct sDATAheader
{
	char ChunkID[4];		// "data"
	int ChunkSize;			// NumSamples * NumChannels * BitsPerSample/8
};

// structure for libFLAC stream handling
struct sClientData
{
	FILE* fin;
	FILE* fout;
	FLAC__uint64 total_samples;
	unsigned int sample_rate;
	unsigned int channels;
	unsigned int bps;
	unsigned int blocksize;
};

// structure for Encode2FLAC / Encode2WAV / Encode2MP3 parameters
struct sEncodingParameters
{
	bool ThreadInUse;					// is thread in use?
	WCHAR filename[MAXFILENAMELENGTH];	// name of the file to be encoded
	HWND progresstotal;					// handle for the total progress bar
	HWND progress;						// handle for thread's progress bar
	HWND text;							// handle for the static text
};

// structure for EncoderScheduler parameters
struct sUIParameters
{
	bool EncoderInUse;			// is encoding running?
	HDROP filedrop;				// handle for dropped files
	HWND progresstotal;			// handle for the total progress bar
	HWND progress[MAX_THREADS];	// handle for each thread's progress bar
	HWND text;					// handle for the static text
};

// callback functions for libFLAC encoder stream handling
FLAC__StreamEncoderWriteStatus write_callback_2FLAC(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data);
FLAC__StreamEncoderSeekStatus seek_callback_2FLAC(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data);
FLAC__StreamEncoderTellStatus tell_callback_2FLAC(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
void metadata_callback_2FLAC(const FLAC__StreamEncoder *encoder, const FLAC__StreamMetadata *metadata, void *client_data);

// callback functions for libFLAC decoder stream handling
FLAC__StreamDecoderWriteStatus write_callback_2WAV(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
FLAC__StreamDecoderReadStatus read_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
FLAC__StreamDecoderSeekStatus seek_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
FLAC__StreamDecoderTellStatus tell_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
FLAC__StreamDecoderLengthStatus length_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
FLAC__bool eof_callback_2WAV(const FLAC__StreamDecoder *decoder, void *client_data);
void metadata_callback_2WAV(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
void error_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);