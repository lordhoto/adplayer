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

#ifndef MUSIC_H
#define MUSIC_H

#include "adplayer.h"

class MusicPlayer : public Player {
public:
	MusicPlayer(const FileBuffer &file);

	virtual bool isPlaying() const;
protected:
	virtual void callback();
private:
	void noteOff(uint8_t channel);
	uint8_t findFreeChannel();
	void setupFrequency(uint8_t channel, int8_t frequency);
	void setupRhythm(uint8_t rhythmInstr, uint16_t instrOffset);

	bool _isPlaying;

	uint16_t _timerLimit;
	uint16_t _musicTicks;
	uint16_t _musicTimer;
	bool _loopFlag;
	uint16_t _musicLoopStart;
	uint16_t _instrumentOffset[16];
	uint8_t _channelLastEvent[9];
	uint8_t _channelFrequency[9];
	uint8_t _channelB0Reg[9];

	uint8_t _mdvdrState;
	uint8_t _voiceChannels;

	uint16_t _curOffset;
	uint16_t _nextEventTimer;

	static const uint16_t _noteFrequencies[12];
	static const uint8_t _mdvdrTable[6];
	static const uint8_t _rhythmOperatorTable[6];
	static const uint8_t _rhythmChannelTable[6];
};

#endif
