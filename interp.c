#include <stdio.h>
#include <stdlib.h>

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define CHECK_MAGIC(a,b) ((*(int *)(&(a))) == (*(int *)(&(b))))
#define SET_MAGIC(a,b) ((*(int *)(&(a))) = (*(int *)(&(b))))

typedef struct
{
	char  chunkId[4];
	int   chunkSize;
	char  format[4];
	char  subChunk1Id[4];
	int   subChunk1Size;
	short audioFormat;
	short numChannels;
	int   sampleRate;
	int   byteRate;
	short blockAlign;
	short bitsPerSample;
	char  subChunk2Id[4];
	int   subChunk2Size;
} wavHeader;

static short g_NumChannels;
static int g_SampleRate;
static int g_TotalSize;

static inline int parseWavHeader(FILE *f)
{
	wavHeader hdr;
	fread(&hdr, sizeof(wavHeader), 1, f);
	
	if (!CHECK_MAGIC(hdr.chunkId, "RIFF") ||
		hdr.audioFormat   != 1  ||
		hdr.bitsPerSample != 16 ||
		hdr.numChannels > 2)
	{
		return 0;
	}
	
	g_NumChannels = hdr.numChannels;
	g_SampleRate  = hdr.sampleRate;
	g_TotalSize   = hdr.subChunk2Size;
	
	return 1;
}

static inline void writeWavHeader(FILE *f)
{
	wavHeader hdr = {0};
	
	SET_MAGIC(hdr.chunkId, "RIFF");
	hdr.chunkSize = (g_TotalSize * 2) + sizeof(wavHeader);
	SET_MAGIC(hdr.format, "WAVE");
	
	SET_MAGIC(hdr.subChunk1Id, "fmt ");
	hdr.subChunk1Size = 16;
	hdr.audioFormat   = 1;
	hdr.numChannels   = g_NumChannels;
	hdr.sampleRate	  = g_SampleRate * 2;
	hdr.byteRate      = g_SampleRate * 2 * g_NumChannels * 2;
	hdr.blockAlign	  = g_NumChannels * 2;
	hdr.bitsPerSample = 16;
	
	SET_MAGIC(hdr.subChunk2Id, "data");
	hdr.subChunk2Size = g_TotalSize * 2;
	
	fwrite(&hdr, sizeof(wavHeader), 1, f);
}

int main(int argc, char **argv)
{
	FILE *in, *out;
	void *inBuf, *outBuf;
	short *inPtr, *outPtr, sampleHistory[2] = {0};

	if (argc != 3)
	{
		printf("Usage: %s <input wav> <output wav>\n", argv[0]);
		return 0;
	}
	
	if ((in = fopen(argv[1], "rb")) == NULL)
	{
		perror("Error");
		return 1;
	}
	
	if ((out = fopen(argv[2], "wb")) == NULL)
	{
		perror("Error");
		return 1;
	}
	
	if (!parseWavHeader(in))
	{
		fputs("Error: Invalid WAV file\n", stderr);
		return 1;
	}
	
	inBuf  = malloc(g_TotalSize);
	outBuf = malloc(g_TotalSize * 2);
	
	fread(inBuf, 1, g_TotalSize, in);
	fclose(in);
	
	inPtr  = (short *)inBuf;
	outPtr = (short *)outBuf;
	
	printf("Interpolating %s (%d Hz) to %s (%d Hz)...\n", argv[1], g_SampleRate, argv[2], g_SampleRate * 2);
	
	if (g_NumChannels == 1)
	{
		for (int i = 0; i < g_TotalSize; i += sizeof(short))
		{
			short sample = *inPtr++;
			
			short min = MIN(sample, *sampleHistory);
			short max = MAX(sample, *sampleHistory);
			
			/* calculate the average of the previous and current sample */
			*outPtr++ = min + ((max - min) / 2);
			*sampleHistory = *outPtr++ = sample;
		}		
	}
	else /* 2 channel WAV */
	{
		for (int i = 0; i < g_TotalSize; i += sizeof(short) * 2)
		{
			short sampleL = *inPtr++, sampleR = *inPtr++;
			
			short minL = MIN(sampleL, sampleHistory[0]);
			short maxL = MAX(sampleL, sampleHistory[0]);
			
			short minR = MIN(sampleR, sampleHistory[1]);
			short maxR = MAX(sampleR, sampleHistory[1]);
			
			*outPtr++ = minL + ((maxL - minL) / 2);
			*outPtr++ = minR + ((maxR - minR) / 2);
			sampleHistory[0] = *outPtr++ = sampleL;
			sampleHistory[1] = *outPtr++ = sampleR;
		}
	}
	
	writeWavHeader(out);
	
	fwrite(outBuf, 1, g_TotalSize * 2, out);
	fclose(out);
	
	free(inBuf);
	free(outBuf);
	
	puts("Done!");
	
	return 0;
}