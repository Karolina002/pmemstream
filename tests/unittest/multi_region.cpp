// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <rapidcheck.h>

#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

size_t count_max_regions(test_config_type &test_config)
{
	pmemstream_test_base stream(test_config.filename, test_config.block_size, test_config.stream_size);
	size_t max_allocations = 0;
	auto [ret, region] = stream.helpers.stream.region_allocate(test_config.region_size);
	while (ret != -1) {
		std::tie(ret, region) = stream.helpers.stream.region_allocate(test_config.region_size);
		++max_allocations;
	};
	return max_allocations;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = argv[1];
	test_config.stream_size = TEST_DEFAULT_STREAM_SIZE * 10;

	return run_test(test_config, [&] {
		return_check ret;
		size_t max_allocations = count_max_regions(test_config);
		UT_ASSERTne(max_allocations, 0);

		ret += rc::check("Each of allocated regions can be iterated and freed", [&](pmemstream_empty &&stream) {
			/* rc::gen::inRange works in <min, max) range */
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations + 1);

			auto regions = stream.helpers.allocate_regions(no_regions, test_config.region_size);
			RC_ASSERT(no_regions == stream.helpers.count_regions());

			auto iterated = stream.helpers.get_regions();
			RC_ASSERT(regions.size() == iterated.size());
			RC_ASSERT(std::equal(regions.begin(), regions.end(), iterated.begin()));

			stream.helpers.remove_regions(regions);

			RC_ASSERT(0 == stream.helpers.count_regions());
		});

		ret += rc::check(
			"Re-allocated regions can be iterated in expected order", [&](pmemstream_empty &&stream) {
				size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations + 1);

				/* allocate 'no_regions' and remove them */
				auto regions_0 = stream.helpers.allocate_regions(no_regions, test_config.region_size);
				RC_ASSERT(no_regions == stream.helpers.count_regions());

				stream.helpers.remove_regions(regions_0);
				RC_ASSERT(0 == stream.helpers.count_regions());

				/* re-allocate regions removed from first to last - they will be reused in reversed
				 * order */
				auto regions_1 = stream.helpers.allocate_regions(no_regions, test_config.region_size);
				RC_ASSERT(no_regions == stream.helpers.count_regions());

				auto iterated_1 = stream.helpers.get_regions();
				RC_ASSERT(regions_1.size() == iterated_1.size());
				/* use here rbegin & rend for region_0 */
				RC_ASSERT(std::equal(regions_0.rbegin(), regions_0.rend(), regions_1.begin()));
				RC_ASSERT(std::equal(regions_0.rbegin(), regions_0.rend(), iterated_1.begin()));

				/* remove regions in now (already) reversed order */
				stream.helpers.remove_regions(iterated_1);
				RC_ASSERT(0 == stream.helpers.count_regions());

				/* re-allocate regions again, expecting them in "original" order */
				auto regions_2 = stream.helpers.allocate_regions(no_regions, test_config.region_size);
				RC_ASSERT(no_regions == stream.helpers.count_regions());

				auto iterated_2 = stream.helpers.get_regions();
				RC_ASSERT(regions_2.size() == iterated_2.size());
				RC_ASSERT(std::equal(regions_0.begin(), regions_0.end(), regions_2.begin()));
				RC_ASSERT(std::equal(regions_0.begin(), regions_0.end(), iterated_2.begin()));
			});

		ret += rc::check(
			"Some of first/last allocated regions can be freed",
			[&](pmemstream_empty &&stream, bool free_heads) {
				size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations + 1);
				size_t to_delete = *rc::gen::inRange<std::size_t>(1, no_regions);

				auto regions = stream.helpers.allocate_regions(no_regions, test_config.region_size);
				RC_ASSERT(no_regions == stream.helpers.count_regions());

				std::vector<struct pmemstream_region> to_remove;
				if (free_heads) {
					to_remove = std::vector<struct pmemstream_region>(
						regions.begin(), regions.begin() + static_cast<long>(to_delete));
				} else {
					to_remove = std::vector<struct pmemstream_region>(
						regions.rbegin(), regions.rbegin() + static_cast<long>(to_delete));
				}
				stream.helpers.remove_regions(to_remove);

				RC_ASSERT(no_regions - to_delete == stream.helpers.count_regions());
			});

		ret += rc::check("Random region can be freed", [&](pmemstream_empty &&stream) {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations + 1);
			size_t to_delete_pos = *rc::gen::inRange<std::size_t>(0, no_regions);

			auto regions = stream.helpers.allocate_regions(no_regions, test_config.region_size);
			RC_ASSERT(no_regions == regions.size());
			RC_ASSERT(no_regions == stream.helpers.count_regions());

			stream.helpers.remove_region(stream.helpers.get_region(to_delete_pos).offset);
			RC_ASSERT(no_regions - 1 == stream.helpers.count_regions());
		});

		ret += rc::check("Regions can be allocated after some was freed", [&](pmemstream_empty &&stream) {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations + 1);

			auto regions = stream.helpers.allocate_regions(no_regions, test_config.region_size);
			RC_ASSERT(no_regions == stream.helpers.count_regions());

			/* remove random (unique) regions */
			auto to_delete_poss =
				*rc::gen::unique<std::vector<size_t>>(rc::gen::inRange<size_t>(0, no_regions));
			RC_PRE(to_delete_poss.size() > 0);

			std::vector<struct pmemstream_region> to_delete_regs;
			for (auto i : to_delete_poss) {
				to_delete_regs.push_back(stream.helpers.get_region(i));
			}

			stream.helpers.remove_regions(to_delete_regs);
			auto iterated = stream.helpers.get_regions();
			RC_ASSERT(no_regions - to_delete_regs.size() == iterated.size());

			/* allocate again, some extra number of regions */
			size_t no_realloc_regions = *rc::gen::inRange<std::size_t>(0, to_delete_regs.size());

			auto it = to_delete_regs.rbegin();
			for (size_t i = 0; i < no_realloc_regions; i++) {
				auto [ret, region] = stream.helpers.stream.region_allocate(test_config.region_size);
				RC_ASSERT(ret == 0);

				/* and check if regions are re-used in reversed order (comparing to freeing order) */
				RC_ASSERT(region.offset == it->offset);
				iterated.push_back(*it);
				it++;
			}
			RC_ASSERT(no_regions - to_delete_regs.size() + no_realloc_regions ==
				  stream.helpers.count_regions());

			auto re_iterated = stream.helpers.get_regions();
			RC_ASSERT(iterated.size() == re_iterated.size());
			RC_ASSERT(std::equal(iterated.begin(), iterated.end(), re_iterated.begin()));
		});
	});
}
