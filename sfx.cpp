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

#include "sfx.h"

#include <cstring>

SfxPlayer::SfxPlayer(const FileBuffer &file)
    : Player(file), _isPlaying(false), _timer(4), _rndSeed(1) {
	writeReg(0xBD, 0x00);

	int startChannel = _file.at(1) * 3;
	// byte 0 is the priority of the SFX

	std::memset(_channelState, 0, sizeof(_channelState));
	_channelState[startChannel+0] = 0;
	_channelState[startChannel+1] = 0;
	_channelState[startChannel+2] = 0;

	clearChannel(startChannel+0);
	clearChannel(startChannel+1);
	clearChannel(startChannel+2);

	_channelCurrentOffset[startChannel] = _channelStartOffset[startChannel] = 2;
	_channelState[startChannel] = 1;

	int curChannel = startChannel + 1;
	int bufferPosition = 2;
	uint8_t command = 0;
	while ((command = _file.at(bufferPosition)) != 0xFF) {
		switch (command) {
		case 1:
			bufferPosition += 15;
			break;

		case 2:
			bufferPosition += 11;
			break;

		case 0x80:
			bufferPosition += 1;
			break;

		default:
			bufferPosition += 1;
			_channelCurrentOffset[curChannel] = bufferPosition;
			_channelStartOffset[curChannel] = bufferPosition;
			_channelState[curChannel] = 1;
			++curChannel;
			break;
		}
	}

	_isPlaying = true;
}

bool SfxPlayer::isPlaying() const {
	return _isPlaying;
}

void SfxPlayer::callback() {
	if (--_timer)
		return;

	_timer = 4;
	bool stillPlaying = false;

	for (int i = 0; i <= 9; ++i) {
		if (!_channelState[i])
			continue;

		stillPlaying = true;
		updateChannel(i);
	}

	_isPlaying = stillPlaying;
}

void SfxPlayer::clearChannel(int channel) {
	writeReg(0xA0 + channel, 0x00);
	writeReg(0xB0 + channel, 0x00);
}

void SfxPlayer::updateChannel(int channel) {
	if (_channelState[channel] == 1)
		parseSlot(channel);
	else
		updateSlot(channel);
}

void SfxPlayer::parseSlot(int channel) {
	while (true) {
		int curOffset = _channelCurrentOffset[channel];

		switch (_file.at(curOffset)) {
		case 1:
			++curOffset;
			_instrumentData[channel * 7 + 0] = _file.at(curOffset + 0);
			_instrumentData[channel * 7 + 1] = _file.at(curOffset + 2);
			_instrumentData[channel * 7 + 2] = _file.at(curOffset + 9);
			_instrumentData[channel * 7 + 3] = _file.at(curOffset + 8);
			_instrumentData[channel * 7 + 4] = _file.at(curOffset + 4);
			_instrumentData[channel * 7 + 5] = _file.at(curOffset + 3);
			_instrumentData[channel * 7 + 6] = 0;

			setupChannel(channel, curOffset);

			writeReg(0xA0 + channel, _file.at(curOffset + 0));
			writeReg(0xB0 + channel, _file.at(curOffset + 1) & 0xDF);

			_channelCurrentOffset[channel] += 15;
			break;

		case 2:
			++curOffset;
			_channelState[channel] = 2;
			noteOffOn(channel);
			parseNote(channel, 0, curOffset);
			parseNote(channel, 1, curOffset);
			return;

		case 0x80:
			_channelCurrentOffset[channel] = _channelStartOffset[channel];
			break;

		default:
			clearChannel(channel);
			_channelState[channel] = 0;
			// The original had logic to unlock the sound here, if all sfx
			// channels were unused.
			return;
		}
	}
}

void SfxPlayer::updateSlot(int channel) {
	int curOffset = _channelCurrentOffset[channel] + 1;

	for (int num = 0; num <= 1; ++num, curOffset += 5) {
		if (!(_file.at(curOffset) & 0x80))
			continue;

		const int note = channel * 2 + num;
		bool updateNote = false;

		if (_notes[note].state == 2) {
			if (!--_notes[note].sustainTimer)
				updateNote = true;
		} else {
			updateNote = processNoteEnvelope(note, _notes[note].instrumentValue);

			if (_notes[note].bias)
				writeRegisterSpecial(note, _notes[note].bias - _notes[note].instrumentValue, curOffset);
			else
				writeRegisterSpecial(note, _notes[note].instrumentValue, curOffset);
		}

		if (updateNote) {
			if (processNote(note, curOffset)) {
				if (!(_file[curOffset] & 0x08)) {
					_channelCurrentOffset[channel] += 11;
					_channelState[channel] = 1;
					continue;
				} else if (_file[curOffset] & 0x10) {
					noteOffOn(channel);
				}

				_notes[note].state = -1;
				processNote(note, curOffset);
			}
		}

		if ((_file[curOffset] & 0x20) && !--_notes[note].playTime) {
			_channelCurrentOffset[channel] += 11;
			_channelState[channel] = 1;
		}
	}
}

void SfxPlayer::parseNote(int channel, int num, int offset) {
	if (num)
		offset += 5;

	if (_file.at(offset) & 0x80) {
		const int note = channel * 2 + num;
		_notes[note].state = -1;
		processNote(note, offset);
		_notes[note].playTime = 0; 

		if (_file[offset] & 0x20) {
			_notes[note].playTime = (_file.at(offset + 4) >> 4) * 118;
			_notes[note].playTime += (_file[offset + 4] & 0x0F) * 8;
		}
	}
}

bool SfxPlayer::processNote(int note, int offset) {
	if (++_notes[note].state == 4)
		return true;

	const int instrumentDataOffset = _file.at(offset) & 0x07;
	_notes[note].bias = _noteBiasTable[instrumentDataOffset];

	uint8_t instrumentDataValue = 0;
	if (_notes[note].state == 0)
		instrumentDataValue = _instrumentData[(note / 2) * 7 + instrumentDataOffset];

	uint8_t noteInstrumentValue = readRegisterSpecial(note, instrumentDataValue, instrumentDataOffset);
	if (_notes[note].bias)
		noteInstrumentValue = _notes[note].bias - noteInstrumentValue;
	_notes[note].instrumentValue = noteInstrumentValue;

	if (_notes[note].state == 2) {
		_notes[note].sustainTimer = _numStepsTable[_file.at(offset + 3) >> 4];

		if (_file[offset] & 0x40)
			_notes[note].sustainTimer = (((getRnd() << 8) * _notes[note].sustainTimer) >> 16) + 1;
	} else {
		int timer1, timer2;
		if (_notes[note].state == 3) {
			timer1 = _file.at(offset + 3) & 0x0F;
			timer2 = 0;
		} else {
			timer1 = _file.at(offset + _notes[note].state + 1) >> 4;
			timer2 = _file.at(offset + _notes[note].state + 1) & 0x0F;
		}

		int adjustValue = ((_noteAdjustTable[timer2] * _noteAdjustScaleTable[instrumentDataOffset]) >> 16) - noteInstrumentValue;
		setupNoteEnvelopeState(note, _numStepsTable[timer1], adjustValue);
	}

	return false;
}

void SfxPlayer::noteOffOn(int channel) {
	const uint8_t regValue = readReg(0xB0 | channel);
	writeReg(0xB0 | channel, regValue & 0xDF);
	writeReg(0xB0 | channel, regValue | 0x20);
}

void SfxPlayer::writeRegisterSpecial(int note, uint8_t value, int offset) {
	const int dataOffset = _file.at(offset) & 0x07;
	if (dataOffset == 6)
		return;

	note &= 0xFE;

	uint8_t regNum;
	if (_useOperatorTable[dataOffset])
		// This looks like a possible out of bounds read...
		regNum = _operatorOffsetTable[_channelOperatorOffsetTable[dataOffset] + note];
	else
		regNum = _channelOffsetTable[note / 2];

	regNum += _baseRegisterTable[dataOffset];

	uint8_t regValue = readReg(regNum) & (~_registerMaskTable[dataOffset]);
	regValue |= value << _registerShiftTable[dataOffset];

	writeReg(regNum, regValue);
}

uint8_t SfxPlayer::readRegisterSpecial(int note, uint8_t defaultValue, int offset) {
	if (offset == 6)
		return 0;

	note &= 0xFE;

	uint8_t regNum;
	if (_useOperatorTable[offset])
		// This looks like a possible out of bounds read...
		regNum = _operatorOffsetTable[_channelOperatorOffsetTable[offset] + note];
	else
		regNum = _channelOffsetTable[note / 2];

	regNum += _baseRegisterTable[offset];

	uint8_t regValue;
	if (defaultValue)
		regValue = defaultValue;
	else
		regValue = readReg(regNum);

	regValue &= _registerMaskTable[offset];
	regValue >>= _registerShiftTable[offset];

	return regValue;
}

void SfxPlayer::setupNoteEnvelopeState(int note, int steps, int adjust) {
	_notes[note].preIncrease = 0;
	if (std::abs(adjust) > steps) {
		_notes[note].preIncrease = 1;
		_notes[note].adjust = adjust / steps;
		_notes[note].envelope.stepIncrease = std::abs(adjust % steps);
	} else {
		_notes[note].adjust = adjust;
		_notes[note].envelope.stepIncrease = std::abs(adjust);
	}

	_notes[note].envelope.step = steps;
	_notes[note].envelope.stepCounter = 0;
	_notes[note].envelope.timer = steps;
}

bool SfxPlayer::processNoteEnvelope(int note, int &instrumentValue) {
	if (_notes[note].preIncrease)
		instrumentValue += _notes[note].adjust;

	_notes[note].envelope.stepCounter += _notes[note].envelope.stepIncrease;
	if (_notes[note].envelope.stepCounter >= _notes[note].envelope.step) {
		_notes[note].envelope.stepCounter -= _notes[note].envelope.step;

		if (_notes[note].adjust < 0)
			--instrumentValue;
		else
			++instrumentValue;
	}

	if (--_notes[note].envelope.timer)
		return false;
	else
		return true;
}

uint8_t SfxPlayer::getRnd() {
	if (_rndSeed & 1) {
		_rndSeed >>= 1;
		_rndSeed ^= 0xB8;
	} else {
		_rndSeed >>= 1;
	}

	return _rndSeed;
}

const uint8_t SfxPlayer::_noteBiasTable[7] = {
	0x00, 0x00, 0x3F, 0x00, 0x3F, 0x00, 0x00
};

const uint16_t SfxPlayer::_numStepsTable[16] = {
	    1,    4,    6,    8,
	   10,   14,   18,   24,
	   36,   64,  100,  160,
	  240,  340,  600, 1200
};

const uint8_t SfxPlayer::_noteAdjustScaleTable[7] = {
	255,   7,  63,  15,  63,  15,  63
};

const uint16_t SfxPlayer::_noteAdjustTable[16] = {
	    0,  4369,  8738, 13107,
	17476, 21845, 26214, 30583,
	34952, 39321, 43690, 48059,
	52428, 46797, 61166, 65535
};

const bool SfxPlayer::_useOperatorTable[7] = {
	false, false, true, true, true, true, false
};

const uint8_t SfxPlayer::_channelOffsetTable[11] = {
	 0,  1,  2,  3,
	 4,  5,  6,  7,
	 8,  8,  7
};

const uint8_t SfxPlayer::_channelOperatorOffsetTable[7] = {
	0, 0, 1, 1, 0, 0, 0
};

const uint8_t SfxPlayer::_baseRegisterTable[7] = {
	0xA0, 0xC0, 0x40, 0x20, 0x40, 0x20, 0x00
};

const uint8_t SfxPlayer::_registerMaskTable[7] = {
	0xFF, 0x0E, 0x3F, 0x0F, 0x3F, 0x0F, 0x00
};

const uint8_t SfxPlayer::_registerShiftTable[7] = {
	0, 1, 0, 0, 0, 0, 0
};
