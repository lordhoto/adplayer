/* adplayer - A player for SCUMM AD resource files.
 *
 * (c) 2011 by Johannes Schickel <lordhoto at gmail dot com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdint.h>
#include <stdexcept>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <iterator>

typedef std::vector<uint8_t> FileBuffer;
void loadADFile(const std::string &filename, FileBuffer &data);
void validateADFile(FileBuffer &data);

int main(int argc, char *argv[]) {
	if (argc != 2)
		return -1;

	try {
		FileBuffer data;

		std::string filename(argv[1]);
		loadADFile(filename, data);
		validateADFile(data);
	} catch (const std::exception &e) {
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -1;
	}

	return EXIT_SUCCESS;
}

void loadADFile(const std::string &filename, FileBuffer &data) {
	data.clear();

	struct FileWrapper {
		typedef FILE *FilePtr;
		FilePtr file;

		FileWrapper(const std::string &filename)
		    : file(std::fopen(filename.c_str(), "rb")) {
			if (!file)
				throw std::runtime_error("Could not load file: " + filename);
		}
		~FileWrapper() { std::fclose(file); }

		operator FilePtr() {
			return file;
		}
	};

	FileWrapper input(filename);
	if (std::fseek(input, 0, SEEK_END) == -1)
		throw std::runtime_error("Seeking failed");
	long size = std::ftell(input);
	if (size == -1)
		throw std::runtime_error("Could not determine file size");
	if (std::fseek(input, 0, SEEK_SET) == -1)
		throw std::runtime_error("Seeking failed");

	data.reserve(size);
	uint8_t buffer[4096];
	while (size > 0) {
		size_t bytesRead = std::fread(buffer, 1, sizeof(buffer), input);
		if (!bytesRead)
			throw std::runtime_error("Reading from file failed");
		std::copy(buffer, buffer + bytesRead, std::back_inserter(data));
		size -= bytesRead;
	}
}

void validateADFile(FileBuffer &data) {
	try {
		const bool isMusicFile = (data.at(2) == 0x80);
		if (isMusicFile) {
		} else {
			if (data.at(1) > 3)
				throw std::runtime_error("SFX start channel is too big");
		}
	} catch (const std::range_error &e) {
		throw std::runtime_error("Premature end of file");
	}
}
