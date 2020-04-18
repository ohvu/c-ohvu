#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uchar.h>
#include <unicode/utf.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>

#include "c-calipto/stream.h"

void close_stream(stream* s) {
	s->close(s);
}

typedef struct ustring_stream {
	UChar* start;
	UChar* end;
} ustring_stream;

block* next_ustring_block(stream* ss) {
	ustring_stream* uss = (ustring_stream*)(ss + 1);

	if (uss->start == NULL) {
		return NULL;
	}

	if (uss->end == NULL) {
		uss->end = uss->start;
		while (*uss->end != u'\0') {
			uss->end++;
		}
	}

	block* b = malloc(sizeof(block));
	b->start = uss->start;
	b->end = uss->end;

	uss->start = NULL;

	return b;
}

void free_ustring_block(stream* s, block* b) {
	free(b);
}

void close_ustring_stream(stream* s) {
	free(s);
}

stream* open_ustring_stream(UChar* s) {
	stream* ss = malloc(sizeof(stream) + sizeof(ustring_stream));
	ustring_stream* uss = (ustring_stream*)(ss + 1);

	ss->next_block = next_ustring_block;
	ss->free_block = free_ustring_block;
	ss->close = close_ustring_stream;

	uss->start = s;
	uss->end = NULL;

	return ss;
}

stream* open_nustring_stream(UChar* s, int64_t l) {
	stream* ss = open_ustring_stream(s);
	ustring_stream* uss = (ustring_stream*)(ss + 1);

	uss->end = s + l;

	return ss;
}

typedef struct file_stream {
	FILE* file;
} file_stream;

block* next_file_block(stream* s) {
	return NULL;
}

void free_file_block(stream*s, block* b) {
	;
}

void close_file_stream(stream* s) {
	free(s);
}

stream* open_file_stream(FILE* f) {
	stream* s = malloc(sizeof(stream) + sizeof(file_stream));
	file_stream* fs = (file_stream*)(s + 1);

	s->next_block = next_file_block;
	s->free_block = free_file_block;
	s->close = close_file_stream;

	fs->file = f;

	return s;
}
