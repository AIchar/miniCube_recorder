#ifndef _ARZHE_WAVE_H_
#define _ARZHE_WAVE_H_

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;

typedef struct WAV_RIFF {
    /* chunk "riff" */
    char ChunkID[4];   /* "RIFF" */
    /* sub-chunk-size */
    uint32_t ChunkSize; /* 36 + Subchunk2Size */
    /* sub-chunk-data */
    char Format[4];    /* "WAVE" */
} RIFF_t;

typedef struct WAV_FMT {
    /* sub-chunk "fmt" */
    char Subchunk1ID[4];   /* "fmt " */
    /* sub-chunk-size */
    uint32_t Subchunk1Size; /* 16 for PCM */
    /* sub-chunk-data */
    uint16_t AudioFormat;   /* PCM = 1*/
    uint16_t NumChannels;   /* Mono = 1, Stereo = 2, etc. */
    uint32_t SampleRate;    /* 8000, 44100, etc. */
    uint32_t ByteRate;  /* = SampleRate * NumChannels * BitsPerSample/8 */
    uint16_t BlockAlign;    /* = NumChannels * BitsPerSample/8 */
    uint16_t BitsPerSample; /* 8bits, 16bits, etc. */
} FMT_t;

typedef struct WAV_data {
    /* sub-chunk "data" */
    char Subchunk2ID[4];   /* "data" */
    /* sub-chunk-size */
    uint32_t Subchunk2Size; /* data size */
    /* sub-chunk-data */
//    Data_block_t block;
} Data_t;

//typedef struct WAV_data_block {
//} Data_block_t;

typedef struct WAV_fotmat {
   RIFF_t riff;
   FMT_t fmt;
   Data_t data;
} Wav;

#define l2b_32(a) ((((uint32_t)a & 0x000000FF) << 24 | \
                    ((uint32_t)a & 0x0000FF00) << 8 | \
                    ((uint32_t)a & 0x00FF0000) >> 8 | \
                    ((uint32_t)a & 0xFF000000) >> 24))

#define l2b_16(a) ((uint16_t)a<< 8 | (uint16_t)a>>8)

#endif