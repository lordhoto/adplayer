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

#ifndef ADPLAYER_H
#define ADPLAYER_H

#include <vector>
#include <stdint.h>
#include <boost/scoped_ptr.hpp>
#include <SDL.h>
#include "dbopl.h"

typedef std::vector<uint8_t> FileBuffer;

class Player {
public:
	Player(const FileBuffer &file);
	virtual ~Player();

	virtual bool isPlaying() const = 0;
protected:
	FileBuffer _file;

	virtual void callback() = 0;

	void writeReg(uint16_t reg, uint8_t data);
private:
	typedef boost::scoped_ptr<DBOPL::Chip> ChipPtr;
	ChipPtr _emulator;
	SDL_AudioSpec _obtained;

	const int _callbackFrequency;
	int32_t _samplesPerCallback;
	int32_t _samplesPerCallbackRemainder;
	int32_t _samplesTillCallback;
	int32_t _samplesTillCallbackRemainder;

	static void readSamples(void *userdata, Uint8 *buffer, int len);
};

#endif
