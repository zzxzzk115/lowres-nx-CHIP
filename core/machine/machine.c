//
// Copyright 2016-2018 Timo Kloss
//
// This file is part of LowRes NX.
//
// LowRes NX is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// LowRes NX is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with LowRes NX.  If not, see <http://www.gnu.org/licenses/>.
//

#include "machine.h"
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "core.h"

void machine_init(struct Core *core)
{
    assert(sizeof(struct Machine) == 0x10000);
}

void machine_reset(struct Core *core)
{
    memset(core->machine, 0, sizeof(struct Machine));
    memset(core->machineInternals, 0, sizeof(struct MachineInternals));
    audio_reset(core);
}

int machine_peek(struct Core *core, int address)
{
    if (address < 0 || address > 0xFFFF)
    {
        return -1;
    }
    if (address >= 0xE000 && address < 0xF000) // persistent
    {
        if (!core->machineInternals->hasAccessedPersistent)
        {
            delegate_persistentRamWillAccess(core, core->machine->persistentRam, PERSISTENT_RAM_SIZE);
            core->machineInternals->hasAccessedPersistent = true;
        }
    }
    
    // read byte
    return *(uint8_t *)((uint8_t *)core->machine + address);
}

bool machine_poke(struct Core *core, int address, int value)
{
    if (address < 0x8000 || address > 0xFFFF)
    {
        // cartridge ROM or outside RAM
        return false;
    }
    if (address >= 0xF000 && address < 0xFE00)
    {
        // reserved memory
        return false;
    }
    if (address >= 0xFF80)
    {
        // reserved registers
        return false;
    }
    if (address == 0xFF76) // IO attributes
    {
        // check for illegal input change (gamepad <-> touch)
        union IOAttributes currAttr = core->machine->ioRegisters.attr;
        union IOAttributes newAttr;
        newAttr.value = value;
        if (currAttr.gamepadsEnabled > 0 && (newAttr.gamepadsEnabled == 0 || newAttr.touchEnabled))
        {
            return false;
        }
        if (currAttr.touchEnabled && (newAttr.touchEnabled == 0 || newAttr.gamepadsEnabled > 0))
        {
            return false;
        }
    }
    else if (address >= 0xE000 && address < 0xF000) // persistent
    {
        if (!core->machineInternals->hasAccessedPersistent)
        {
            delegate_persistentRamWillAccess(core, core->machine->persistentRam, PERSISTENT_RAM_SIZE);
            core->machineInternals->hasAccessedPersistent = true;
        }
        core->machineInternals->hasChangedPersistent = true;
    }
    
    // write byte
    *(uint8_t *)((uint8_t *)core->machine + address) = value & 0xFF;
    
    if (address == 0xFF76) // IO attributes
    {
        delegate_controlsDidChange(core);
    }
    else if (address >= 0xFF40 && address < 0xFF70) // audio
    {
        machine_enableAudio(core);
    }
    return true;
}

void machine_enableAudio(struct Core *core)
{
    if (!core->machineInternals->audioInternals.audioEnabled)
    {
        core->machineInternals->audioInternals.audioEnabled = true;
        delegate_controlsDidChange(core);
    }
}

void machine_suspendEnergySaving(struct Core *core, int numUpdates)
{
    if (core->machineInternals->energySavingTimer < numUpdates)
    {
        core->machineInternals->energySavingTimer = numUpdates;
    }
}
