//=====================================================================================================================================
//	The MIT License (MIT)
//	Copyright (c) 2015 Austin Salgat
//	
//	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files
//	(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify,
//	merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished
//	to do so, subject to the following conditions:
//	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
//	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
//	LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
//	IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//=====================================================================================================================================

#pragma once
#ifndef SERIALIZE_QUEUE_H
#define SERIALIZE_QUEUE_H

#include <array>
#include <vector>
#include <stack>
#include <memory>
#include <fstream>
#include <inttypes.h>
#include <cstring>
#include <string>
#include <iomanip>
#include <utility>
#include <map>
#include <queue>
#include <stack>
#include <tuple>
#include <stdexcept>

#include <iostream>

namespace serq {
	/**
	 * Returns a reversed tuple. (thanks to http://stackoverflow.com/questions/25119048/reversing-a-c-tuple)
	 */
	template<typename T, typename TT = typename std::remove_reference<T>::type, size_t... I>
	auto reverse_impl(T&& t, std::index_sequence<I...>) -> std::tuple<typename std::tuple_element<sizeof...(I) - 1 - I, TT>::type...> {
		return std::make_tuple(std::get<sizeof...(I) - 1 - I>(std::forward<T>(t))...);
	}

	template<typename T, typename TT = typename std::remove_reference<T>::type>
	auto reverse(T&& t) -> decltype(reverse_impl(std::forward<T>(t), std::make_index_sequence<std::tuple_size<TT>::value>())) {
		return reverse_impl(std::forward<T>(t), std::make_index_sequence<std::tuple_size<TT>::value>());
	}

	template <class T>
	struct tag {};

	/**
	 * FIFO queue that is able to convert back and forth from and to binary (serialized) data.
	 */
	class SerializeQueue {
	private:
		std::vector<uint64_t> variable_lengths; // Used to store the lengths of variable sized objects
		std::stack<std::vector<unsigned char>> binary_data;
		
		std::vector<unsigned char> original_serialized_blob;
		std::vector<unsigned char> serialized_blob;
		
		uint32_t checksum; // Holds the checksum of the last deserialized character blob
		//uint32_t offset;  // Holds the byte offset of the last deserialized character blob
		uint32_t const kSerializeOffset = 0; // Holds the byte offset used for serializing
		uint64_t serialized_header_length; // Holds length of data header (variable_lengths) for serialized data
		
		// Helper methods
		/**
		 * Adds the char blob to the serialized_blob.
		 */
		void PushBlob(std::vector<unsigned char> const& char_blob) {
			for (unsigned char const character : char_blob) {
				serialized_blob.push_back(character);
			}
		}
		
		void PushBlob(std::vector<unsigned char> const& char_blob, int offset) {
			//for (unsigned char const character : char_blob) {
			for (int index = offset; index < char_blob.size(); ++index) {
				serialized_blob.push_back(char_blob[index]);
			}
		}
		
		/**
		 * Pushes data directly to serialized_data.
		 */
		void PushToSerializedBlob(uint64_t const data) {
			for (std::size_t index = 0; index < sizeof(data); ++index) {
				serialized_blob.push_back((data >> (index*8)) & 0xFF);
			}
		}
		
		/**
		 * Returns uint64_t value at provided serialized_blob offset.
		 */
		uint64_t ReadSerializedBlob(int const offset) const {
			uint64_t data = 0x00;
			for (int index = sizeof(data)-1; index >= 0; --index) {
				unsigned char character = serialized_blob[index+offset];
				data |= static_cast<uint64_t>(character) << static_cast<uint64_t>(index*8);
			}
			
			return *reinterpret_cast<uint64_t*>(&data);
		}

		uint64_t ReadSerializedBlob(int const offset, std::vector<unsigned char> const& data_vector) const {
			uint64_t data = 0x00;
			for (int index = sizeof(data)-1; index >= 0; --index) {
				unsigned char character = data_vector[index+offset];
				data |= static_cast<uint64_t>(character) << static_cast<uint64_t>(index*8);
			}
			
			return *reinterpret_cast<uint64_t*>(&data);
		}			
		
		void WriteToOffset(uint64_t const data, int const offset) {
			for (int index = 0; index < sizeof(data); ++index) {
				serialized_blob[index+offset] = (data >> (index*8)) & 0xFF;
			}
		}
	
		/**
		 * Returns length of current variable length object (that is not yet popped).
		 */
		uint64_t GetVariableLength() const {
			auto offset_counter = ReadSerializedBlob(0);
			return ReadSerializedBlob((offset_counter)*8);
		}
		
		/**
		 * Decrements offset counter for variable length variables.
		 */
		void DecrementVariableLengthCounter() {
			auto offset_counter = ReadSerializedBlob(0);
			--offset_counter;
			WriteToOffset(offset_counter, 0x00);
		}
		
		// Push Section -----------------------------------------------------------
		
		/**
		 * Converts data to a character array and pushes it on top of the data stack.
		 */
		void push(uint64_t const data) {
			std::vector<unsigned char> char_blob;
			for (std::size_t index = 0; index < sizeof(data); ++index) {
				char_blob.push_back((data >> (index*8)) & 0xFF);
			}
			
			binary_data.push(char_blob);
		}
		 
		void push(unsigned int const data) {
			// Data is expanded to 64-bit for compatibility with both x64 and x32
			auto data_64bit = static_cast<uint64_t>(data);
			push(data_64bit);
		}
		
		void push(int const data) {
			auto data_64bit = static_cast<int64_t>(data);
			push(*reinterpret_cast<uint64_t*>(&data_64bit));
		}
		
		void push(char data) {
			unsigned char converted_data = *reinterpret_cast<unsigned char*>(&data);
			push(converted_data);
		}
		
		void push(unsigned char const data) {
			std::vector<unsigned char> char_blob = {data};
			binary_data.push(char_blob);
		}

		void push(float const data) {
			float data_copy = data;
			uint64_t data_64bit = 0x00;
			data_64bit |= *reinterpret_cast<uint64_t*>(&data_copy);
			push(data_64bit);
		}
		
		void push(double const data) {
			double data_copy = data;
			if (sizeof(double) == 8) {
				push(*reinterpret_cast<uint64_t*>(&data_copy));
			} else {
				uint64_t data_64bit = 0x00;
				data_64bit |= *reinterpret_cast<uint64_t*>(&data_copy);
				push(data_64bit);
			}	
		}
		
		void push(char const* data) {
			std::vector<unsigned char> char_blob;
			char_blob.push_back('\0');
			for (std::size_t index = 0; data[index] != '\0'; ++index) {
				char_blob.push_back(data[index]);
			}
		
			binary_data.push(char_blob);
		}
		
		void push(std::string const& data) {
			char const* char_array = data.c_str();
			push(char_array);
		}
		
		void push(bool const data) {
			// Todo: Change this to be stored in char (8 bit) when support is added for it.
			uint64_t data_64bit = data ? 0x01 : 0x00;
			push(data_64bit);
		}
		
		template<class T1, class T2>
		void push(std::pair<T1, T2> const& data) {
			push<T1>(data.first);
			push<T2>(data.second);
		}
		
		template<class T1, std::size_t T2>
		void push_array(std::array<T1, T2> const& data_array) {
			for (auto const& data : data_array) {
				push(data);
			}
			
			variable_lengths.push_back(data_array.size());
		}
		
		template<class T>
		void push_vector(std::vector<T> const& data_vector) {
			for (auto const& data : data_vector) {
				push(data);
			}
			
			variable_lengths.push_back(data_vector.size());
		}
		
		template<class T1, class T2>
		void push_map(std::map<T1, T2> const& data_map) {
			std::size_t counter = 0;
			for (auto& entry : data_map) {
				push<T1, T2>(entry);
				++counter;
			}
			
			variable_lengths.push_back(counter);
		}
		
		template<class T>
		void push_queue(std::queue<T> data_queue) {
			std::size_t counter = 0;
			while(!data_queue.empty()) {
				push<T>(data_queue.front());
				data_queue.pop();
				++counter;
			}
			
			variable_lengths.push_back(counter);
		}
		
		template<class T>
		void push_stack(std::stack<T> data_stack) {
			// First reverse the stack (due to it being stored in reverse order)
			std::stack<T> stack_reversed;
			while(!data_stack.empty()) {
				stack_reversed.push(data_stack.top());
				data_stack.pop();
			}
		
			std::size_t counter = 0;
			while(!stack_reversed.empty()) {
				push<T>(stack_reversed.top());
				stack_reversed.pop();
				++counter;
			}
			
			variable_lengths.push_back(counter);
		}
		
		void push(tag<uint64_t>, uint64_t const data) {
			push(data);
		}
		
		void push(tag<unsigned int>, unsigned int const data) {
			push(data);
		}
		
		void push(tag<int>, int const data) {
			push(data);
		} 
		
		void push(tag<char>, char const data) {
			push(data);
		}
		 
		void push(tag<unsigned char>, unsigned char const data) {
			push(data);
		}
		
		void push(tag<float>, float const data) {
			push(data);
		}
		
		void push(tag<double>, double const data) {
			push(data);
		}
		
		void push(tag<std::string>, std::string const& data) {
			push(data);
		}
		
		void push(tag<bool>, bool const data) {
			uint64_t data_64bit = data ? 0x01 : 0x00;
			push(data_64bit);
		}
		
		// STL Containers
		
		template<class T1, class T2>
		void push(tag<std::pair<T1, T2>>, std::pair<T1, T2> const& data) {
			push(data);
		}
		
		template<class... Values, std::size_t... IndexSequence>
		void push_tuple(std::tuple<Values...> const& data, std::index_sequence<IndexSequence...>) {
			// Due to how the tuple is stored, it needs to be reversed (pop order is backwards).
			// Todo: A more efficient way to push or pop in reverse
			auto new_tuple = reverse(data);
			(int[]){ 0, (push(std::get<IndexSequence>(new_tuple)), 0)... };
		}
		
		template<class... Values>
		void push(tag<std::tuple<Values...>>, std::tuple<Values...> const& data) {
			
			push_tuple(data, std::make_index_sequence<sizeof...(Values)>());
		}
		
		template<class T1, std::size_t T2>
		void push(tag<std::array<T1, T2>>, std::array<T1, T2> const& data_array) {
			push_array(data_array);
		}
		
		template<class T>
		void push(tag<std::vector<T>>, std::vector<T> const& data_vector) {
			push_vector(data_vector);
		}
		
		template<class T1, class T2>
		void push(tag<std::map<T1, T2>>, std::map<T1, T2> const& data_map) {
			push_map(data_map);
		}
		
		template<class T>
		void push(tag<std::queue<T>>, std::queue<T> data_queue) {
			push_queue(data_queue);
		}
		
		template<class T>
		void push(tag<std::stack<T>>, std::stack<T> data_stack) {
			push_stack(data_stack);
		}
		
		// Pop Section -----------------------------------------------------------
		/**
		 * Specialization of pop() for STL containers. All types supported by T pop are supported in the container.
		 */
		template<class T1, class T2>
		std::pair<T1, T2> pair_pop() {
			T1 first = pop<T1>();
			T2 second = pop<T2>();
			return std::pair<T1, T2>(first, second);
		}
		 
		template<class T1, std::size_t T2>
		std::array<T1, T2> array_pop() {
			std::array<T1, T2> data_array;
			auto length = GetVariableLength();
			for (std::size_t index = 0; index < length; ++index) {
				data_array[index] = pop<T1>();
			}
			
			DecrementVariableLengthCounter();
			return data_array;
		} 
		 
		template<class T>
		std::vector<T> vector_pop() {
			std::vector<T> data_vector;
			auto length = GetVariableLength();
			for (std::size_t index = 0; index < length; ++index) {
				data_vector.push_back(pop<T>());
			}
			
			DecrementVariableLengthCounter();
			return data_vector;
		}
		
		template<class T1, class T2>
		std::map<T1, T2> map_pop() {
			std::map<T1, T2> data_map;
			auto length = GetVariableLength();
			std::pair<T1, T2> entry;
			for (std::size_t index = 0; index < length; ++index) {
				entry = pop<std::pair<T1, T2>>();
				data_map.insert(entry);
			}
			
			DecrementVariableLengthCounter();
			return data_map;
		}
		
		template<class T>
		std::queue<T> queue_pop() {
			std::queue<T> data_queue;
			auto length = GetVariableLength();
			T entry;
			for (std::size_t index = 0; index < length; ++index) {
				entry = pop<T>();
				data_queue.push(entry);
			}
			
			DecrementVariableLengthCounter();
			return data_queue;
		}
		
		template<class T>
		std::stack<T> stack_pop() {
			std::stack<T> data_stack;
			auto length = GetVariableLength();
			T entry;
			for (std::size_t index = 0; index < length; ++index) {
				entry = pop<T>();
				data_stack.push(entry);
			}
			
			DecrementVariableLengthCounter();
			return data_stack;
		}
		
		template<class T>
		T pop_generic() {
			uint64_t data = 0x00;
			for (int index = sizeof(data)-1; index >= 0; --index) {
				if (static_cast<int>(serialized_blob.size()) - static_cast<int>(serialized_header_length) <= 0) {
					throw std::out_of_range("Popping byte beyond end of serialized data.");
				}
			
				unsigned char character = serialized_blob.back();
				serialized_blob.pop_back();
				data |= static_cast<uint64_t>(character) << static_cast<uint64_t>(index*8);
			}
			
			return *reinterpret_cast<T*>(&data);
		}
		
		/**
		 * 
		 */
		uint64_t pop(tag<uint64_t>) {
			return pop_generic<uint64_t>();
		}
		
		unsigned int pop(tag<unsigned int>) {
			return pop_generic<unsigned int>();
		}
		
		int pop(tag<int>) {
			return pop_generic<int>();
		}
		
		char pop(tag<char>) {
			if (static_cast<int>(serialized_blob.size()) - static_cast<int>(serialized_header_length) <= 0) {
				throw std::out_of_range("Popping byte beyond end of serialized data.");
			}
			
			unsigned char character = serialized_blob.back();
			serialized_blob.pop_back();
			
			return *reinterpret_cast<char*>(&character);
		}
		
		unsigned char pop(tag<unsigned char>) {
			if (static_cast<int>(serialized_blob.size()) - static_cast<int>(serialized_header_length) <= 0) {
				throw std::out_of_range("Popping byte beyond end of serialized data.");
			}
			
			unsigned char character = serialized_blob.back();
			serialized_blob.pop_back();
			
			return character;
		}
		
		float pop(tag<float>) {
			return pop_generic<float>();
		}
		
		double pop(tag<double>) {
			return pop_generic<double>();
		}
		
		bool pop(tag<bool>) {
			uint64_t result = pop_generic<uint64_t>();
			return (result > 0);
		}
		
		// STL Containers
		
		template<class T1, class T2>
		std::pair<T1, T2> pop(tag<std::pair<T1, T2>>) {
			return pair_pop<T1, T2>();
		}
		
		template<class... Values>
		auto pop(tag<std::tuple<Values...>>) {
			return std::make_tuple<Values...>(std::move(pop<Values>())...);
		}
		
		template<class T1, std::size_t T2>
		std::array<T1, T2> pop(tag<std::array<T1, T2>>) {
			return array_pop<T1, T2>();
		}
		
		template<class T>
		std::vector<T> pop(tag<std::vector<T>>) {
			return vector_pop<T>();
		}
		
		template<class T1, class T2>
		std::map<T1, T2> pop(tag<std::map<T1, T2>>) {
			return map_pop<T1, T2>();
		}
		
		template<class T>
		std::queue<T> pop(tag<std::queue<T>>) {
			return queue_pop<T>();
		}
		
		template<class T>
		std::stack<T> pop(tag<std::stack<T>>) {
			return stack_pop<T>();
		}
		
		/**
		 * Serialized File Integrity Functions (CRC-32)
		 *
		 * Credit: Michael Barr and his instructional series on CRC
		 *         http://www.barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code
		 */
		// CRC32 table for generator polynomial 0x04C11DB7
		uint32_t crc_table[256] = {
		0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
	    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
	    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
	    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
	    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
	    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
	    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
	    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
	    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
	    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
	    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
	    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
	    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
	    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
	    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
	    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
	    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
	    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
	    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
	    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
	    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
	    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
	    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
	    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
	    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
	    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
	    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
	    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
	    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
	    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
	    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
	    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
	    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
	    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
	    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
	    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
	    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
	    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
	    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
	    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
	    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
	    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
	    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
	    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
	    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
	    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
	    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
	    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
	    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
	    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};
		
		/**
		 * Creates a CRC checksum based on the serialized blob provided.
		 */
		uint32_t CalculateCheckSum(std::vector<unsigned char> const& character_blob) {
			uint32_t remainder = 0xFFFFFFFF;
			
			// Divide the message by the polynomial, a byte at a time.
			for (std::size_t byte = 0; byte < character_blob.size(); ++byte) {
				remainder = (remainder >> 8) ^ crc_table[(remainder ^ character_blob[byte]) & 0xFF];
			}

			return (remainder ^ 0xFFFFFFFF);
		}
		
	public:
		SerializeQueue() {}
		
		/**
		 * Clears queue completely.
		 */
		void Clear() {
			variable_lengths.clear(); // Used to store the lengths of variable sized objects
			binary_data = std::stack<std::vector<unsigned char>>();
			
			original_serialized_blob.clear();
			serialized_blob.clear();
		}
		
		/**
		 * Returns a vector of char which represents the binary state of the serialized data.
		 * Data is stored in little-endian order (least significant byte in the smallest address).
		 *
		 * Note: vector is used because it automatically stores the size of the char blob.
		 */
		std::vector<unsigned char> Serialize() {
			serialized_blob.clear();
			
			// Create the header, outlined as follows (all entrys are 64 bits)
			//  - Address 0x00: Header length (this is decremented whenever variable data is popped)
			//    - Used to determine the length of a variable object, example for std::vector<int>
			//		uint64_t vector_length = current_variable_length(); // this function needs to read the first 8 bytes as a uint64_t
			//      decrement_variable_length();
			// Todo: Take the original_serialized_blob and use its header, modified, for the new serialized_blob
			
			// Get original header, and append new header data to it
			unsigned int original_header_length;
			if (original_serialized_blob.size() > 0) {
				original_header_length = ReadSerializedBlob(0x00, original_serialized_blob); // Multiply by 8 for the character length
			} else {
				original_header_length = 0;
			}
			unsigned int new_header_length = original_header_length + variable_lengths.size();
			PushToSerializedBlob(static_cast<uint64_t>(new_header_length));
			
			if (original_header_length > 0) {
				for (std::size_t index = 1*8; index < (original_header_length+1)*8; ++index) {
					serialized_blob.push_back(original_serialized_blob[index]);
				}
			}
			for (std::size_t index = 0 ; index < variable_lengths.size(); ++index) {
				PushToSerializedBlob(variable_lengths[variable_lengths.size()-1-index]);
			}
			
			// Append original data minus the old header
			for (std::size_t index = (new_header_length+1)*8; index < original_serialized_blob.size(); ++index) {
				serialized_blob.push_back(original_serialized_blob[index]);
			}
			
			// Append the new data
			auto binary_data_copy = binary_data;
			while(!binary_data_copy.empty()) {
				auto const char_blob = binary_data_copy.top();
				binary_data_copy.pop();
				
				PushBlob(char_blob);
			}
			
			return serialized_blob;
		}
		
		/**
		 * Stores a binary blob of the serialized data to the provided file name and returns the
		 * character blob in a vector.
		 */
		std::vector<unsigned char> Serialize(std::string const& file_name) {
			Serialize();
			uint32_t checksum = CalculateCheckSum(serialized_blob);
			
			// Write char array to file
			std::ofstream output(file_name, std::ios::out | std::ios::binary);
			if (!output)
				throw std::ofstream::failure("Failed to create output file.");
			
			// Initialize Header
			// Address 0x00-0x03 = CRC32 Checksum
			output.put(static_cast<unsigned char>(checksum&0xFF));
			output.put(static_cast<unsigned char>((checksum>>8)&0xFF));
			output.put(static_cast<unsigned char>((checksum>>16)&0xFF));
			output.put(static_cast<unsigned char>((checksum>>24)&0xFF));
			
			// Append character blob to file
			for (auto const& character : serialized_blob) {
				output.put(character);
			}
			
			return serialized_blob;
		}
		
		/**
		 * Resets the values of this class object to whatever contents are in the provided binary blob.
		 */
		void Deserialize(std::string const& file_name) {
			Clear();
		
			std::ifstream input(file_name, std::ios::in | std::ios::binary);
			if (!input)
				throw std::ifstream::failure("Failed to open file.");
			
			// Read CRC32 checksum
			checksum = 0;
			char character;
			input.get(character); checksum += static_cast<uint32_t>(static_cast<unsigned char>(character));
			input.get(character); checksum += static_cast<uint32_t>(static_cast<unsigned char>(character)) << 8;
			input.get(character); checksum += static_cast<uint32_t>(static_cast<unsigned char>(character)) << 16;
			input.get(character); checksum += static_cast<uint32_t>(static_cast<unsigned char>(character)) << 24;
			
			// Read in queue data
			while(input.get(character)) {
				original_serialized_blob.push_back(static_cast<unsigned char>(character));
			}
			
			serialized_blob = original_serialized_blob;
			original_serialized_blob.clear();
			
			serialized_header_length = static_cast<uint64_t>(serialized_blob[0]);
			serialized_header_length += static_cast<uint64_t>(serialized_blob[1]) << 8;
			serialized_header_length += static_cast<uint64_t>(serialized_blob[2]) << 16;
			serialized_header_length += static_cast<uint64_t>(serialized_blob[3]) << 24;
			serialized_header_length += static_cast<uint64_t>(serialized_blob[4]) << 32;
			serialized_header_length += static_cast<uint64_t>(serialized_blob[5]) << 40;
			serialized_header_length += static_cast<uint64_t>(serialized_blob[6]) << 48;
			serialized_header_length += static_cast<uint64_t>(serialized_blob[7]) << 56;
			serialized_header_length += 1; // An extra uint64_t for the storing of the data header length
			serialized_header_length *= 8;
		}
		
		/**
		 * Returns true if checksum for deserialized data is valid (data integrity kept).
		 *
		 * NOTE: This function must only be used after void Deserialize() and before
		 *       any other operations which may invalidate checksum.
		 */
		bool ValidateData() {
			uint32_t current_checksum = CalculateCheckSum(serialized_blob);
			return (current_checksum == checksum) ? true : false;
		}
		
		/**
		 * Generic push onto queue (calls one of the other specific implementations).
		 *
		 * Note: Tag-dispatching is used to allow for STL template specializations
		 */
		template<class T>
		void push(T data) {
			push(tag<T>(), data);
		}
		
		/**
		 * Depending on the size of the data type, removes from the char blob data to form data type.
		 */
		template<class T>
		T pop() {
			return pop(tag<T>());
		}
	};

	// Template specializations
	//--------------------------------------------------------------------------------------------------------------
	
	/**
	 * Specialization of pop() meant for std::string.
	 * 
	 * Note: Stored in the serial blob as a character array wrapped with a null terminator on both sides.
	 */
	template<>
	std::string SerializeQueue::pop<std::string>() {
		std::vector<unsigned char> char_array;
		for (char character = serialized_blob.back(); character != '\0'; character = serialized_blob.back()) {
			if (static_cast<int>(serialized_blob.size()) - static_cast<int>(serialized_header_length) <= 0) {
				throw std::out_of_range("Popping byte beyond end of serialized data.");
			}
			
			char_array.push_back(character);
			serialized_blob.pop_back();
		}
		if (static_cast<int>(serialized_blob.size()) - static_cast<int>(serialized_header_length) <= 0) {
			throw std::out_of_range("Popping byte beyond end of serialized data.");
		}
				
		serialized_blob.pop_back();
	
		// Create a c string that is the reverse
		char c_string[char_array.size()+1];
		for (int index = char_array.size(); index > 0; --index) {
			c_string[char_array.size() - index] = char_array[index-1];
			
		}
		c_string[char_array.size()] = '\0';
		
		return std::string(c_string);
	}
}	

/**
 * TODO:
 *	- Add support for raw binaries (character blobs that don't use null terminators).
 *  - std::map and other stl containers
 *  - Exception support.
 *		- A better approach may be to use asserts instead, for things like reading beyond the end of the char blob.
 *	- Use move semantics where appropriate to avoid extra copies
 */


#endif // SERIALIZE_QUEUE
