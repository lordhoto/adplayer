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

#include "music.h"

#include <cstring>
#include <stdexcept>

MusicPlayer::MusicPlayer(const FileBuffer &file)
    : Player(file) {
	_musicTicks = _file.at(3);
	_loopFlag = (_file.at(4) == 0);
	_musicLoopStart = readWord(5);

	std::memset(_instrumentOffset, 0, sizeof(_instrumentOffset));
	std::memset(_channelLastEvent, 0, sizeof(_channelLastEvent));
	std::memset(_channelFrequency, 0, sizeof(_channelFrequency));
	std::memset(_channelB0Reg, 0, sizeof(_channelB0Reg));

	_voiceChannels = 0;

	const uint8_t instruments = _file[10];
	for (uint8_t i = 0; i < instruments; ++i) {
		const int instrIndex = _file.at(11 + i) - 1;
		if (0 <= instrIndex && instrIndex < 16) {
			_instrumentOffset[instrIndex] = i * 16 + 16 + 3;
			_voiceChannels |= _file.at(_instrumentOffset[instrIndex] + 13);
		}
	}

	if (_voiceChannels) {
		_mdvdrState = 0x20;
		_voiceChannels = 6;
	} else {
		_mdvdrState = 0;
		_voiceChannels = 9;
	}

	_curOffset = 0x93;
	_nextEventTimer = 40;

	_musicTimer = 0;

	writeReg(0xBD, _mdvdrState);

	_isPlaying = true;
}

bool MusicPlayer::isPlaying() const {
	return _isPlaying;
}

void MusicPlayer::callback() {
	_musicTimer += _musicTicks;
	if (_musicTimer <= 0xFF)
		return;

	_musicTimer -= 0x100;

	--_nextEventTimer;
	if (_nextEventTimer)
		return;

	while (true) {
		uint8_t command = _file.at(_curOffset++);
		if (command == 0xFF) {
			command = _file.at(_curOffset++);
			if (command == 47) {
				// End of track
				_isPlaying = false;
				return;
			} else if (command == 88) {
				_curOffset += 6;
			}
		} else {
			if (command >= 0x90) {
				command -= 0x90;

				const uint16_t instrOffset = _instrumentOffset[command];
				if (instrOffset) {
					if (_file.at(instrOffset + 13) != 0) {
						setupRhythm(_file[instrOffset + 13], instrOffset);
					} else {
						uint8_t channel = findFreeChannel();
						if (channel != 0xFF) {
							noteOff(channel);
							setupChannel(channel, instrOffset);
							_channelLastEvent[channel] = command + 0x90;
							_channelFrequency[channel] = _file.at(_curOffset);
							setupFrequency(channel, _file[_curOffset]);
						}
					}
				}
			} else {
				const uint8_t note = _file.at(_curOffset);
				command += 0x10;

				uint8_t channel = 0xFF;
				for (uint8_t i = 0; i < _voiceChannels; ++i) {
					if (_channelFrequency[i] == note && _channelLastEvent[i] == command) {
						channel = i;
						break;
					}
				}

				if (channel != 0xFF) {
					noteOff(channel);
				} else {
					command -= 0x90;
					const uint16_t instrOffset = _instrumentOffset[command];
					if (instrOffset && _file.at(instrOffset + 13) != 0) {
						const uint8_t rhythmInstr = _file[instrOffset + 13];
						//if (rhythmInstr >= 6)
						//	throw std::range_error("rhythmInstr >= 6");
						if (rhythmInstr < 6) {
							_mdvdrState &= _mdvdrTable[rhythmInstr] ^ 0xFF;
							writeReg(0xBD, _mdvdrState);
						}
					}
				}
			}

			_curOffset += 2;
		}

		if (_file.at(_curOffset) != 0)
			break;
		++_curOffset;
	}

	_nextEventTimer = _file.at(_curOffset++);
	if (_nextEventTimer & 0x80) {
		_nextEventTimer -= 0x80;
		_nextEventTimer <<= 7;
		_nextEventTimer |= _file.at(_curOffset++);
	}

	_nextEventTimer >>= 1;
	if (!_nextEventTimer)
		_nextEventTimer = 1;
}

void MusicPlayer::noteOff(uint8_t channel) {
	_channelLastEvent[channel] = 0;
	writeReg(0xB0 + channel, _channelB0Reg[channel] & 0xDF);
}

uint8_t MusicPlayer::findFreeChannel() {
	for (uint8_t i = 0; i < _voiceChannels; ++i) {
		if (!_channelLastEvent[i])
			return i;
	}

	return 0xFF;
}

void MusicPlayer::setupFrequency(uint8_t channel, int8_t frequency) {
	frequency -= 31;
	if (frequency < 0)
		frequency = 0;

	uint8_t octave = 0;
	while (frequency >= 12) {
		frequency -= 12;
		++octave;
	}

	const uint16_t noteFrequency = _noteFrequencies[frequency];
	octave <<= 2;
	octave |= noteFrequency >> 8;
	octave |= 0x20;
	writeReg(0xA0 + channel, noteFrequency & 0xFF);
	_channelB0Reg[channel] = octave;
	writeReg(0xB0 + channel, octave);
}

void MusicPlayer::setupRhythm(uint8_t rhythmInstr, uint16_t instrOffset) {
	if (rhythmInstr == 1) {
		setupChannel(6, instrOffset);
		writeReg(0xA6, _file.at(instrOffset++));
		writeReg(0xB6, _file.at(instrOffset) & 0xDF);
		_mdvdrState |= 0x10;
		writeReg(0xBD, _mdvdrState);
	} else if (rhythmInstr < 6) {
		uint16_t secondOperatorOffset = instrOffset + 8;
		setupOperator(_rhythmOperatorTable[rhythmInstr], secondOperatorOffset);
		writeReg(0xA0 + _rhythmChannelTable[rhythmInstr], _file.at(instrOffset++));
		writeReg(0xB0 + _rhythmChannelTable[rhythmInstr], _file.at(instrOffset++) & 0xDF);
		writeReg(0xC0 + _rhythmChannelTable[rhythmInstr], _file.at(instrOffset));
		_mdvdrState |= _mdvdrTable[rhythmInstr];
		writeReg(0xBD, _mdvdrState);
	} else {
		// throw range error
	}
}

const uint16_t MusicPlayer::_noteFrequencies[12] = {
	0x200, 0x21E, 0x23F, 0x261,
	0x285, 0x2AB, 0x2D4, 0x300,
	0x32E, 0x35E, 0x390, 0x3C7
};

const uint8_t MusicPlayer::_mdvdrTable[6] = {
	0x00, 0x10, 0x08, 0x04, 0x02, 0x01
};

const uint8_t MusicPlayer::_rhythmOperatorTable[6] = {
	0x00, 0x00, 0x14, 0x12, 0x15, 0x11
};

const uint8_t MusicPlayer::_rhythmChannelTable[6] = {
	0x00, 0x00, 0x07, 0x08, 0x08, 0x07
};
