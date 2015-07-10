SerializeQueue
=================
SerializeQueue (serq) is a C++14 header only library that supports serializing data and STL containers using a queue. Data is pushed onto the queue and is popped in the same order. It is the responsibility of the programmer to ensure that push and pop order is correct, otherwise data will be deserialized incorrectly. Additionally, data integrity is stored using CRC32 and can be optionally checked upon deserialization.

SerializeQueue supports the following data types,
* bool
* char, unsigned char
* uint64_t
* int, unsigned int
* float, double
* std::string
* STL Containers (which can hold any of the supported types, including other STL containers)
  * std::pair<T1, T2>
  * std::tuple<T...> (limited to basic types)
  * std::vector<T>
  * std::map<T>
  * std::queue<T>
  * std::stack<T>

There are a few caveats to using this library. 
* SerializeQueue is cross compatible between 32 and 64 bit (a serialized value will work on either architecture); however, values larger than 2^32-1 used on a 32-bit platform should stick to uint64_t types. 
* Data is serialized assuming little-endianness. 
* Due to overload ambiguities, explicit support for uint8_t, uint16_t, and uint32_t is not supported. These values should be cast to unsigned int before storing, and cast back after popping. The issue comes down to the compiler treating types like uint32_t and unsigned int as identical (probably due to an internal typedef), which results in two overloads having the same type (which is invalid). The same applies for int8_t, etc, which should be cast to and from int.
* std::tuple<T> has limited support (only basic data types). Hopefully once I get a better handle on template metaprogramming, I can fix this.

Example
-----------------

#### Write
```c++
#include "serq.hpp"

int main() {
	// Game data to save
	std::map<std::string, unsigned int> high_scores;
	high_scores["Jim"] = 932;
	high_scores["John"] = 732;
	high_scores["Bob"] = 1023;
	
	std::vector<std::vector<int>> game_map = {{1, 0, 0},
											  {0, 1, 0},
											  {0, 1, 1}};
											  
	double game_version = 1.4;

	// Create a SerializeQueue to push data onto
	serq::SerializeQueue save_data;
	save_data.push<std::map<std::string, unsigned int>>(high_scores);
	save_data.push<std::vector<std::vector<int>>>(game_map);
	save_data.push<double>(game_version);
	
	// Serialize data into binary file
	save_data.Serialize("data.serq");
}
```

#### Read
```c++
#include <iostream>

#include "serq.hpp"

int main() {
	// Deserialize saved data
	serq::SerializeQueue save_data;
	save_data.Deserialize("data.serq");
	
	// Data validation (optional) *must* be done after deserialization but before any other
	// actions on the class.
	if (save_data.ValidateData()) {
		std::cout << "Passed file integrity check" << std::endl;
	} else {
		std::cout << "Failed file integrity check" << std::endl;
	}
	
	// Pop data from queue
	auto high_scores = save_data.pop<std::map<std::string, unsigned int>>();
	auto game_map = save_data.pop<std::vector<std::vector<int>>>();
	auto game_version = save_data.pop<double>();
	
	// Display saved data
	std::cout << std::endl << "High Scores: " << std::endl
			  << '\t' << high_scores["Jim"] << std::endl
			  << '\t' << high_scores["John"] << std::endl
			  << '\t' << high_scores["Bob"] << std::endl;
			  
	std::cout << std::endl << "Game Map" << std::endl;
	for (auto& row : game_map) {
		for (auto& entry : row) {
			std::cout << entry << ", ";
		}
		std::cout << std::endl;
	}
	
	std::cout << std::endl << "Game Version: " << game_version << std::endl;
}
```

#### Output
```
High Scores: 
	932
	732
	1023

Game Map
1, 0, 0, 
0, 1, 0, 
0, 1, 0, 

Game Version: 1.2
```

Todo
-----------------
* Add support for more STL containers
* Expand std::tuple<T...> support
* Setup tests (TDD)
* Optimize (move semantics, etc)
* Perhaps create a way for people to overload their own object types easily?
