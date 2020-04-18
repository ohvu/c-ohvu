#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>
#include <locale.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/ucnv.h>

#include "c-calipto/sexpr.h"
#include "c-calipto/stream.h"
#include "c-calipto/scanner.h"
#include "c-calipto/reader.h"

bool always(char32_t c) {
	return true;
}

int main(int argc, char** argv) {
	setlocale(LC_ALL, "");
	UErrorCode error = 0;
	UConverter* char_conv = ucnv_open(NULL, &error);

	sexpr* args = sexpr_nil();
	for (int i = argc - 1; i >= 0; i--) {
		sexpr* arg = sexpr_string(char_conv, argv[i]);
		sexpr* rest = args;

		args = sexpr_cons(arg, rest);

		sexpr_free(arg);
		sexpr_free(rest);
	}

	sexpr_dump(args);

	FILE* f = fopen("./bootstrap.cal", "r");
	stream* st = open_file_stream(f);
	scanner* sc = open_scanner(st);
	reader* r = open_reader(sc);

	;

	close_reader(r);
	close_scanner(sc);
	close_stream(st);
	fclose(f);

	// TODO evaluate bootstrap file

	sexpr_free(args);

	ucnv_close(char_conv);
	return 0;
}
