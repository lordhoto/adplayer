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

#include "adplayer.h"
#include "music.h"
#include "sfx.h"

#include <stdexcept>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <algorithm>

void loadADFile(const std::string &filename, FileBuffer &data);
void validateADFile(FileBuffer &data);

void outputHelp() {
	std::printf("Usage:\n"
	            "\tadplayer [--loom] input-file\n"
	            "\n"
	            "\t    --loom        Switch for Loom v3 music files\n");
}

int main(int argc, char *argv[]) {
	if (argc != 2 && argc != 3) {
		outputHelp();
		return -1;
	}

	const bool isLoom = (argc == 3 && !std::strcmp(argv[1], "--loom"));

	try {
		FileBuffer data;

		std::string filename(argc == 3 ? argv[2] : argv[1]);
		loadADFile(filename, data);
		validateADFile(data);

		if (SDL_Init(SDL_INIT_AUDIO) == -1)
			throw std::runtime_error("Could not initialize SDL audio subsystem");

		if (data.at(2) == 0x80) {
			MusicPlayer player(data, isLoom);
			player.startPlayback();
			while (player.isPlaying())
				SDL_Delay(100);
			player.stopPlayback();
		} else {
			SfxPlayer player(data);
			player.startPlayback();
			while (player.isPlaying())
				SDL_Delay(100);
			player.stopPlayback();
		}
	} catch (const std::exception &e) {
		std::fprintf(stderr,  "ERROR: %s\n", e.what());
		return -1;
	}

	SDL_Quit();
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

Player::Player(const FileBuffer &file)
    : _file(file), _emulator(new DBOPL::Chip()), _obtained(),
      _callbackFrequency(472), _samplesPerCallback(),
      _samplesPerCallbackRemainder(), _samplesTillCallback(),
      _samplesTillCallbackRemainder() {
	DBOPL::InitTables();

	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.freq = 44100;
	desired.format = AUDIO_S16SYS;
	desired.channels = 1;
	desired.samples = 8192;
	desired.callback = Player::readSamples;
	desired.userdata = static_cast<void *>(this);

	if (SDL_OpenAudio(&desired, &_obtained) < 0)
		throw std::runtime_error("Could not open audio device");
	if (_obtained.format != AUDIO_S16SYS) {
		SDL_CloseAudio();
		throw std::runtime_error("Could not obtain S16SYS audio format");
	}

	_emulator->Setup(_obtained.freq);

	writeReg(0x01, 0x00);
	writeReg(0xBD, 0x00);
	writeReg(0x08, 0x00);
	writeReg(0x01, 0x20);

	_samplesPerCallback = _obtained.freq / _callbackFrequency;
	_samplesPerCallbackRemainder = _obtained.freq % _callbackFrequency;
}

void Player::startPlayback() {
	SDL_PauseAudio(0);
}

void Player::stopPlayback() {
	SDL_PauseAudio(1);
	SDL_CloseAudio();
}

void Player::writeReg(uint16_t reg, uint8_t data) {
	_registerBackUpTable[reg] = data;
	_emulator->WriteReg(reg, data);
}

uint16_t Player::readWord(const uint32_t offset) const {
	return static_cast<uint16_t>(_file.at(offset) | (_file.at(offset + 1) << 8));
}

void Player::readSamples(void *userdata, Uint8 *buffer, int len) {
	Player *player = static_cast<Player *>(userdata);
	int16_t *dst = reinterpret_cast<int16_t *>(buffer);

	const int bufferLength = 512;
	int32_t tempBuffer[bufferLength];

	len /= 2;

	while (len > 0) {
		if (!player->_samplesTillCallback) {
			player->callback();
			player->_samplesTillCallback = player->_samplesPerCallback;
			player->_samplesTillCallbackRemainder += player->_samplesPerCallbackRemainder;
			if (player->_samplesTillCallbackRemainder >= player->_callbackFrequency) {
				++player->_samplesTillCallback;
				player->_samplesTillCallbackRemainder -= player->_callbackFrequency;
			}
		}

		const int samplesToRead = std::min(std::min(len, bufferLength), player->_samplesTillCallback);
		player->_emulator->GenerateBlock2(samplesToRead, tempBuffer);

		const int32_t *src = tempBuffer;
		for (int i = 0; i < samplesToRead; ++i)
			*dst++ = *src++ * 435 / 256;

		len -= samplesToRead;
		player->_samplesTillCallback -= samplesToRead;
	}
}

void Player::setupChannel(uint8_t channel, uint16_t instrOffset) {
	instrOffset += 2;
	writeReg(0xC0 + channel, _file.at(instrOffset++));
	setupOperator(_operatorOffsetTable[channel * 2 + 0], instrOffset);
	setupOperator(_operatorOffsetTable[channel * 2 + 1], instrOffset);
}

void Player::setupOperator(uint8_t opr, uint16_t &instrOffset) {
	writeReg(0x20 + opr, _file.at(instrOffset++));
	writeReg(0x40 + opr, _file.at(instrOffset++));
	writeReg(0x60 + opr, _file.at(instrOffset++));
	writeReg(0x80 + opr, _file.at(instrOffset++));
	writeReg(0xE0 + opr, _file.at(instrOffset++));
}

const uint8_t Player::_operatorOffsetTable[18] = {
	 0,  3,  1,  4,
	 2,  5,  8, 11,
	 9, 12, 10, 13,
	16, 19, 17, 20,
	18, 21
};

