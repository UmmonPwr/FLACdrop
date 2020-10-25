#include "stream_encoder.h"
#include "stream_decoder.h"
#include "metadata.h"

#define READSIZE_FLAC 1048576		// size of a block to read from disk in bytes for the FLAC encoder algorithm
#define READSIZE_MP3 8192			// size of a block to read from disk in bytes for the MP3 encoder algorithm
#define SIZE_RAW_BUFFER 32768		// size of raw audio data buffer for transcoding
#define MAXFILENAMELENGTH 1024		// maximum size of a file name with full path
#define EVENTLOGSIZE 65536			// maximum character size of the event log
#define MAXMETADATA 1024			// maximum character size of metadata string

// selectable output types
#define OUT_TYPE_FLAC 0
#define OUT_TYPE_MP3 1
#define OUT_TYPE_WAV 2

// positions of the metadata variables in the transfer structure
#define MD_NUMBER		16			// number of metadata fields
#define MD_TITLE		0
#define MD_VERSION		1
#define MD_ALBUM		2
#define MD_TRACKNUMBER	3
#define MD_DISCNUMBER	4
#define MD_ARTIST		5
#define MD_PERFORMER	6
#define MD_COPYRIGHT	7
#define MD_LICENSE		8
#define MD_ORGANIZATION	9
#define MD_DESCRIPTION	10
#define MD_GENRE		11
#define MD_DATE			12
#define MD_LOCATION		13
#define MD_CONTACT		14
#define MD_ISRC			15

// default values of system variables for libflac, libmp3lame
#define FLAC_ENCODINGQUALITY 6		// 1..8
#define FLAC_MAXENCODINGQUALITY 8
#define FLAC_VERIFY false
#define FLAC_MD5CHECK true
#define LAME_FLUSH true
#define LAME_NOGAP false
#define LAME_CBRBITRATE 10
#define LAME_INTERNALQUALITY 2		// 0..9
#define LAME_MAXINTERNALQUALITY 9	// 0..9
#define LAME_VBRQUALITY 1			// 0..9
#define LAME_MAXVBRQUALITY 9
#define LAME_ENCTYPE 0				// 0: CBR; 1: VBR
#define OUT_TYPE 0					// according to the "OUT_TYPE_*" definitions
#define OUT_THREADS 1				// current number of batch processing threads
#define MAX_THREADS 8				// maximum number of batch processing threads

//WAVE file audio formats
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
#define WAVE_FORMAT_PCM			0x0001	// PCM
#define WAVE_FORMAT_IEEE_FLOAT	0x0003	// IEEE float
#define WAVE_FORMAT_ALAW		0x0006	// 8 - bit ITU - T G.711 A - law
#define WAVE_FORMAT_MULAW		0x0007	// 8 - bit ITU - T G.711 µ - law
#define WAVE_FORMAT_EXTENSIBLE	0xFFFE	// Determined by SubFormat

// libmp3lame encoding bitrates
const TCHAR LAME_CBRBITRATES_TEXT[][4] = {
	TEXT("48"), TEXT("64"), TEXT("80"), TEXT("96"), TEXT("112"), TEXT("128"), TEXT("160"), TEXT("192"), TEXT("224"), TEXT("256"), TEXT("320") };
const int LAME_CBRBITRATES[] = {
	48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
#define LAME_CBRBITRATES_QUANTITY 10	// quantity of "LAME_CBRBITRATES"-1

// failure codes
#define ALL_OK						0
#define FAIL_FILE_OPEN				1
#define FAIL_WAV_BAD_HEADER			2
#define FAIL_WAV_UNSUPPORTED		3
#define FAIL_LIBFLAC_ONLY_16_24_BIT	4
#define FAIL_LIBFLAC_BAD_HEADER		5
#define FAIL_LIBFLAC_ALLOC			6
#define FAIL_LIBFLAC_ENCODE			7
#define FAIL_LIBFLAC_DECODE			8
#define FAIL_LIBFLAC_METADATA		9
#define FAIL_LIBFLAC_RELEASE		10
#define WARN_LIBFLAC_MD5			11
#define FAIL_REGISTRY_OPEN			12
#define FAIL_REGISTRY_WRITE			13
#define FAIL_REGISTRY_READ			14
#define FAIL_LAME_ONLY_16_BIT		15
#define FAIL_LAME_MAX_2_CHANNEL		16
#define FAIL_LAME_INIT				17
#define FAIL_LAME_ID3TAG			18
#define FAIL_LAME_ENCODE			19
#define FAIL_LAME_CLOSE				20

// failure messages for failure codes
const WCHAR ErrMessage[][60] = {
	L"Encoding OK\r\n",											//0
	L"Error during opening the file\r\n",						//1
	L"WAVE: Invalid WAVE file header\r\n",						//2
	L"WAVE: Unsupported WAVE file compression format\r\n",		//3
	L"libflac: Only 16 and 24 bit files are supported\r\n",		//4
	L"libflac: Invalid FLAC file header\r\n",					//5
	L"libflac: Error during allocating libFLAC encoder\r\n",	//6
	L"libflac: Encoding failed\r\n",							//7
	L"libflac: Decoding failed\r\n",							//8
	L"libflac: Failed to decode metadata",						//9
	L"libflac: Failed encoder release",							//10
	L"libflac warning: MD5 hash mismatch",						//11
	L"registry: Open failed\r\n",								//12
	L"registry: Writing failed\r\n",							//13
	L"registry: Reading failed\r\n",							//14
	L"libmp3lame: Only 16 bit files are supported",				//15
	L"libmp3lame: Only mono or stereao files are supported",	//16
	L"libmp3lame: Error during initialization\r\n",				//17
	L"libmp3lame: Error during writing ID3TAG\r\n",				//18
	L"libmp3lame: Error during encoding\r\n",					//19
	L"libmp3lame: Error during closing\r\n"};					//20

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
	UINT OUT_Threads;					// 1..MAX_THREADS
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
	char ChunkID[4];				// "fmt "
	int ChunkSize;					// 16, 18 or 40 bytes
	unsigned short AudioFormat;		// data format code
	short NumChannels;
	int SampleRate;
	int ByteRate;					// SampleRate * NumChannels * BitsPerSample/8
	short BlockAlign;				// NumChannels * BitsPerSample/8
	short BitsPerSample;
	short ExtensionSize;			// Size of the extension (0 or 22 bytes)
	short ValidBitsPerSample;
	int ChannelMask;				// Speaker position mask
	short SubFormat_AudioFormat;	// data format code
	char SubFormat_GUID[14];
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
	BYTE* buffer_out;
	FLAC__uint64 total_samples;
	unsigned int sample_rate;
	unsigned int channels;
	unsigned int bps;
	unsigned int blocksize;
};

// structure for the encoder thread status variables
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
	bool EncoderInUse;			// is the encoding thread running?
	int OUT_Type;				// which output file type to use
	HDROP filedrop;				// handle for dropped files
	HWND progresstotal;			// handle for the total progress bar
	HWND progress[MAX_THREADS];	// handle for each thread's progress bar
	HWND text;					// handle for the static text
};

// structure for metadata transfer
struct sMetaData
{
	char *text;
	bool present;
};

// encoder algorithms
DWORD WINAPI Encode_WAV2FLAC(LPVOID *params);
DWORD WINAPI Encode_WAV2MP3(LPVOID *params);
DWORD WINAPI Encode_FLAC2WAV(LPVOID *params);
DWORD WINAPI Encode_FLAC2MP3(LPVOID *params);

DWORD WINAPI EncoderScheduler(LPVOID *params);