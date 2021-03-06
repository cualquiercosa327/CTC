#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "ctc.h"

size_t rleBytesOf1Encode(unsigned char *destination, unsigned char *source, size_t length) {
	unsigned int i, j = 0, c = 0;
	
	for(i = 0; i < length; i++) {
		if(!c) {
			if(destination) destination[j] = source[i];
			j++;
			
			if(source[i] == 1) c = 1;
		}
		else {
			if(source[i] == 1) c++;
			else {
				if(destination) destination[j] = c - 1;
				j++;
				c = 0;
				
				if(destination) destination[j] = source[i];
				j++;
			}
		}
	}
	
	if(c) {
		if(destination) destination[j] = c - 1;
		j++;
	}
	
	return j;
}

size_t rleBytesOf1Decode(unsigned char *destination, unsigned char *source, size_t sourceLength, size_t maxDestinationLength) {
	unsigned int i, j = 0, c;
	
	for(i = 0; i < sourceLength; i++) {
		if(destination && j < maxDestinationLength) destination[j] = source[i];
		j++;
		
		if(source[i] == 1) {
			if(i + 1 < sourceLength) {
				for(c = 0; c < source[i + 1]; c++) {
					if(destination && j < maxDestinationLength) destination[j] = 1;
					j++;
				}
				i++;
			}
		}
	}
	
	return j;
}

void diffEncode(unsigned char *destination, unsigned char *source, size_t length) {
	unsigned char last = 0;
	int i;
	for(i = 0; i < length; i++) {
		destination[i] = source[i] - last;
		last = source[i];
	}
}

void diffDecode(unsigned char *destination, unsigned char *source, size_t length) {
	unsigned char last = 0;
	int i;
	for(i = 0; i < length; i++) {
		destination[i] = source[i] + last;
		last = destination[i];
	}
}

static void writeBit(unsigned char *byteDest, unsigned char bitOffset, unsigned char bit) {
	if(bit) (*byteDest) |= (1 << (7 - bitOffset));
	else (*byteDest) &= ~(1 << (7 - bitOffset));
}

static void writeBitCode(unsigned char write, unsigned char **byteDest, unsigned char *bitOffset, int n) {
	int i;
	for(i = 0; i < n / 2; i++) {
		if(write) writeBit(*byteDest, *bitOffset, 1);
		
		*bitOffset += 1;
		*byteDest += (*bitOffset) / 8;
		*bitOffset %= 8;
	}
	
	if(write) writeBit(*byteDest, *bitOffset, 0);
	*bitOffset += 1;
	*byteDest += (*bitOffset) / 8;
	*bitOffset %= 8;
	
	if(write) writeBit(*byteDest, *bitOffset, n % 2);
	*bitOffset += 1;
	*byteDest += (*bitOffset) / 8;
	*bitOffset %= 8;
}

static unsigned char readBit(unsigned char *byteDest, unsigned char bitOffset) {
	return ((*byteDest) & (1 << (7 - bitOffset))) >> (7 - bitOffset);
}

static unsigned char readBitCode(unsigned char **byteDest, unsigned char *bitOffset) {
	unsigned char n;
	
	int c = 0;
	unsigned char b;
	
	while(1) {
		b = readBit(*byteDest, *bitOffset);
		
		*bitOffset += 1;
		*byteDest += (*bitOffset) / 8;
		*bitOffset %= 8;
		
		if(b) c++;
		else break;
	}
	
	n = c * 2;
	
	b = readBit(*byteDest, *bitOffset);
		
	*bitOffset += 1;
	*byteDest += (*bitOffset) / 8;
	*bitOffset %= 8;
	
	n += b;
	
	return n;
}

void sortFrequencies(unsigned char *sortedListDestination, unsigned char *frequencyTableSource) {
	int bar = 0;
	int position = 0;
	
	int i;
	for(i = 0; i < 256; i++) {
		if(frequencyTableSource[i] > bar) {
			bar = frequencyTableSource[i];
		}
	}
	
	while(position < 256) {
		int i;
		for(i = 0; i < 256; i++) {
			if(frequencyTableSource[i] == bar) {
				sortedListDestination[position++] = i;
			}
		}
		
		bar--;
	}
}

void getFrequencies(unsigned char *frequencyTable, unsigned char *data, size_t length) {
	memset(frequencyTable, '\0', 256);
	
	int i;
	for(i = 0; i < length; i++) {
		frequencyTable[data[i]]++;
	}
}

static unsigned char getSortedListIndex(unsigned char *sortedList, unsigned char n) {
	int i;
	for(i = 0; i < 256; i++) {
		if(n == sortedList[i]) return i;
	}
	
	return 0;
}

size_t CTC_Compress(unsigned char *destination, unsigned char *source, size_t length) {
	unsigned char write = destination != NULL;
	
	unsigned char frequencyTable[256];
	unsigned char sortedList[256];
	unsigned char diffEncodedList[256];
	
	getFrequencies(frequencyTable, source, length);
	sortFrequencies(sortedList, frequencyTable);
	
	diffEncode(diffEncodedList, sortedList, 256);
	
	unsigned short diffEncodedListRLESize = rleBytesOf1Encode(NULL, diffEncodedList, 256);
	unsigned char *diffEncodedListRLE = malloc(diffEncodedListRLESize);
	rleBytesOf1Encode(diffEncodedListRLE, diffEncodedList, 256);
	
	if(write) {
		memcpy(destination + offsetof(struct ctcHeader, contentLength), &length, 4);
		memcpy(destination + offsetof(struct ctcHeader, tableLength), &diffEncodedListRLESize, 2);
		memcpy(destination + offsetof(struct ctcHeader, tableLength) + 2, diffEncodedListRLE, diffEncodedListRLESize);
	}
	
	destination += offsetof(struct ctcHeader, tableLength) + 2 + diffEncodedListRLESize;
	unsigned char *byteOffset = destination;
	unsigned char bitOffset = 0;
	
	int i;
	for(i = 0; i < length; i++) {
		writeBitCode(write, &byteOffset, &bitOffset, getSortedListIndex(sortedList, source[i]));
	}
	
	// Finish writing last byte
	if(bitOffset != 0) {
		while(bitOffset < 8) {
			writeBit(byteOffset, bitOffset, 0);
			bitOffset++;
		}
	}
	
	free(diffEncodedListRLE);
	
	return offsetof(struct ctcHeader, tableLength) + 2 + diffEncodedListRLESize + (byteOffset - destination) + (bitOffset != 0);
}

size_t CTC_Decompress(unsigned char *destination, unsigned char *source, size_t length) {
	size_t contentLength;
	memcpy(&contentLength, source + offsetof(struct ctcHeader, contentLength), 4);
	
	if(!destination) return contentLength;
	
	unsigned short tableLength;
	memcpy(&tableLength, source + offsetof(struct ctcHeader, tableLength), 2);
	
	unsigned char *diffEncodedListRLE;
	diffEncodedListRLE = malloc(tableLength);
	memcpy(diffEncodedListRLE, source + offsetof(struct ctcHeader, tableLength) + 2, tableLength);
	
	unsigned char diffEncodedList[256];
	if(rleBytesOf1Decode(NULL, diffEncodedListRLE, tableLength, 256) != 256) {
		printf("Error! RLE table does not decompress to 256 bytes!\n");
		free(diffEncodedListRLE);
		return 0;
	}
	
	rleBytesOf1Decode(diffEncodedList, diffEncodedListRLE, tableLength, 256);
	
	free(diffEncodedListRLE);
	
	unsigned char sortedList[256];
	diffDecode(sortedList, diffEncodedList, 256);
	
	unsigned char *byteOffset = source + offsetof(struct ctcHeader, tableLength) + 2 + tableLength;
	unsigned char bitOffset = 0;
	
	int j = 0;
	while(byteOffset - source < length && j < contentLength) {
		unsigned char n = readBitCode(&byteOffset, &bitOffset);
		
		destination[j] = sortedList[n];
		j++;
	}
	
	if(byteOffset - source < length) {
		printf("Warning! Didn't decompress all data.\n");
	}
	
	if(j < contentLength) {
		printf("Warning! Didn't fill decompression buffer.\n");
	}
	
	return j;
}
