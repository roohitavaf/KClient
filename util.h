//
// Created by meox on 21/08/16.
//

#ifndef TICKETKAFKA_UTIL_H
#define TICKETKAFKA_UTIL_H

#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <zmq.hpp>


namespace kutil
{
	void print_time();

	template<typename C, typename T = typename C::value_type>
	std::vector <std::vector<T>> partions(const C &c, uint n)
	{
		//using T = typename C::value_type;
		const size_t l = c.size();
		std::vector <std::vector<T>> ris;

		if (n < l) {
			const size_t d = l / n;
			auto it = c.begin();
			for (size_t i = 0; i < n; i++) {
				std::vector <T> chunk(d);
				std::copy_n(it, d, chunk.begin());
				std::advance(it, d);
				ris.push_back(chunk);
			}

			for (size_t j = 0; it != c.end(); ++it, j = (j + 1) % n) {
				ris[j].push_back(*it);
			}
		} else {
			std::vector <T> chunk;
			std::copy_n(c.begin(), l, chunk.begin());
			ris.push_back(chunk);
		}

		return ris;
	}

	std::string to_string(const zmq::message_t& msg);
}

#endif //TICKETKAFKA_UTIL_H
