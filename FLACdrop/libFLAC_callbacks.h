#include "stream_encoder.h"
#include "stream_decoder.h"

// callback functions for libFLAC encoder stream handling
FLAC__StreamEncoderWriteStatus write_callback_2FLAC(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data);
FLAC__StreamEncoderSeekStatus seek_callback_2FLAC(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data);
FLAC__StreamEncoderTellStatus tell_callback_2FLAC(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
void metadata_callback_2FLAC(const FLAC__StreamEncoder *encoder, const FLAC__StreamMetadata *metadata, void *client_data);

// callback functions for libFLAC decoder stream handling
FLAC__StreamDecoderWriteStatus write_callback_2WAV(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
FLAC__StreamDecoderWriteStatus write_callback_2MEM(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
FLAC__StreamDecoderReadStatus read_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);	// OK also for decoding to memory
FLAC__StreamDecoderSeekStatus seek_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);		// OK also for decoding to memory
FLAC__StreamDecoderTellStatus tell_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);	// OK also for decoding to memory
FLAC__StreamDecoderLengthStatus length_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);		// OK also for decoding to memory
FLAC__bool eof_callback_2WAV(const FLAC__StreamDecoder *decoder, void *client_data);															// OK also for decoding to memory
void metadata_callback_2WAV(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);						// OK also for decoding to memory
void error_callback_2WAV(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);							// OK also for decoding to memory

// callback functions for libFLAC metadata handling in stream
// https://xiph.org/flac/api/group__flac__callbacks.html
size_t read_iocallback(void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle);
size_t write_iocallback(const void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle);
FLAC__int64 tell_iocallback(FLAC__IOHandle handle);
int seek_iocallback(FLAC__IOHandle handle, FLAC__int64 offset, int whence);
int eof_iocallback(FLAC__IOHandle handle);
int close_iocallback(FLAC__IOHandle handle);
