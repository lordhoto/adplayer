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

#ifndef SFX_H
#define SFX_H

#include "adplayer.h"

class SfxPlayer : public Player {
public:
	SfxPlayer(const FileBuffer &file);

	virtual bool isPlaying() const;
protected:
	virtual void callback();
private:
	void clearChannel(int channel);
	void updateChannel(int channel);
	void parseSlot(int channel);
	void updateSlot(int channel);
	void parseNote(int channel, int num, int offset);
	bool processNote(int note, int offset);
	void noteOffOn(int channel);
	void writeRegisterSpecial(int note, uint8_t value, int offset);
	uint8_t readRegisterSpecial(int note, uint8_t defaultValue, int offset);
	void setupNoteEnvelopeState(int note, int steps, int adjust);
	bool processNoteEnvelope(int note, int &instrumentValue);

	bool _isPlaying;

	int _timer;

	int _channelState[11];
	int _channelCurrentOffset[11];
	int _channelStartOffset[11];

	uint8_t _instrumentData[11 * 7];

	int _noteState[22];
	int _notePlayTime[22];
	int _noteSustainTimer[22];
	int _noteInstrumentValue[22];
	int _noteBias[22];
	int _notePreIncrease[22];
	int _noteAdjust[22];
	int _noteEnvelopeStepIncrease[22];
	int _noteEnvelopeStep[22];
	int _noteEnvelopeStepCounter[22];
	int _noteEnvelopeTimer[22];

	static const uint8_t _noteBiasTable[7];
	static const uint16_t _numStepsTable[16];
	static const uint8_t _noteAdjustScaleTable[7];
	static const uint16_t _noteAdjustTable[16];
	static const bool _useOperatorTable[7];
	static const uint8_t _channelOffsetTable[11];
	static const uint8_t _channelOperatorOffsetTable[7];
	static const uint8_t _baseRegisterTable[7];
	static const uint8_t _registerMaskTable[7];
	static const uint8_t _registerShiftTable[7];
};

#endif
