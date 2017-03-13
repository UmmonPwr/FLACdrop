#include "helper_encode.h"

// encoder algorithms
DWORD WINAPI Encode_WAV2FLAC(LPVOID *params);
DWORD WINAPI Encode_WAV2MP3(LPVOID *params);
DWORD WINAPI Encode_FLAC2WAV(LPVOID *params);

DWORD WINAPI EncoderScheduler(LPVOID *params);