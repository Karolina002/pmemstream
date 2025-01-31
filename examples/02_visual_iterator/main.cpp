// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"

#include <cstdio>
#include <libpmem2.h>
#include <string>
#include <vector>

struct data_entry {
	uint64_t data;
};

using std::string;
using std::vector;

static vector<string> inner_pointers = {"├── ", "│   "};

void print_help(const char *exec_filename)
{
	printf("Usage: %s file [--print-as-text]\n", exec_filename);
}

/**
 * This example prints visual representation of stream's content.
 * It requires a path to already existing file, with a previously filled stream data.
 *
 * Possible usage:
 * ./example-01_basic_iterate existing_file
 * ./example-02_visual_iterator existing_file
 */
int main(int argc, char *argv[])
{
	bool values_as_text = false;
	struct pmemstream_region region;

	if (argc < 2 || argc > 3) {
		print_help(argv[0]);
		return -1;
	}
	if (argc == 3) {
		if (string(argv[2]) != "--print-as-text") {
			print_help(argv[0]);
			return -1;
		}
		values_as_text = true;
	}

	/* helper function to open a file if exists, or create it with given size otherwise */
	struct pmem2_map *map = example_map_open(argv[1], EXAMPLE_STREAM_SIZE);
	if (map == NULL) {
		pmem2_perror("pmem2_map");
		return -1;
	}

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, 4096, map);
	if (ret != 0) {
		fprintf(stderr, "pmemstream_from_map failed\n");
		return ret;
	}

	struct pmemstream_region_iterator *riter;
	ret = pmemstream_region_iterator_new(&riter, stream);
	if (ret != 0) {
		fprintf(stderr, "pmemstream_region_iterator_new failed\n");
		return ret;
	}

	pmemstream_region_iterator_seek_first(riter);

	/* Iterate over all regions. */
	size_t region_id = 0;
	while (pmemstream_region_iterator_is_valid(riter) == 0) {
		region = pmemstream_region_iterator_get(riter);
		struct pmemstream_entry_iterator *eiter;
		ret = pmemstream_entry_iterator_new(&eiter, stream, region);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_entry_iterator_new failed\n");
			return ret;
		}
		printf("%s region%ld: %ld bytes\n", inner_pointers[0].data(), region_id++,
		       pmemstream_region_size(stream, region));

		/* Iterate over all elements in a region and save last entry value. */
		for (pmemstream_entry_iterator_seek_first(eiter); pmemstream_entry_iterator_is_valid(eiter) == 0;
		     pmemstream_entry_iterator_next(eiter)) {
			struct pmemstream_entry entry = pmemstream_entry_iterator_get(eiter);
			auto entry_length = pmemstream_entry_length(stream, entry);
			printf("%s%s0x%-3X %ldbytes ", inner_pointers[1].data(), inner_pointers[0].data(),
			       (unsigned int)entry.offset, entry_length);

			if (values_as_text) {
				string entry_text(static_cast<const char *>(pmemstream_entry_data(stream, entry)),
						  entry_length);
				printf("%s", entry_text.c_str());
			} else {
				auto d = static_cast<const unsigned char *>(pmemstream_entry_data(stream, entry));
				for (size_t i = 0; i < entry_length; i++) {
					printf("%.2X ", d[i]);
				}
			}
			printf("\n");
		}
		pmemstream_entry_iterator_delete(&eiter);
		pmemstream_region_iterator_next(riter);
	}

	pmemstream_region_iterator_delete(&riter);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
