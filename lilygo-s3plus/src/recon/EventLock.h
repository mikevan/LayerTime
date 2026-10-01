// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
//
// Copyright (C) 2026 Michael Van Geertruy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

// The S3 Plus event-log lock.
//
// Why it exists: the radios find detections on their own tasks (the Wi-Fi
// driver's promiscuous callback, the NimBLE host's scan callback), and those
// detections go straight into the core's event history
// (src/core/logic/MonitorEventLog). The main loop reads that same history to
// raise alerts and draw the Recon screen. The core documents this as "Not
// thread-safe", a race the T-Ultra carries unchanged. The S3 Plus closes it
// with one mutex, owned here, without changing the core.
//
// Who holds it, and the one rule that keeps it deadlock-free:
//   * Radio side: S3PlusReconService takes it around each frame or
//     advertisement it classifies, so the classifier state and the event
//     history change together.
//   * Main loop: S3PlusApp takes it around core.tick() (alerts), the
//     ReconClearEvents and ReconAcknowledgeAlert commands, and the copy the
//     screens draw from.
//   * The rule: no radio is ever started, stopped, or reconfigured while the
//     lock is held. The radio tasks may be blocked on this lock, and stopping
//     a radio can wait for its task, so holding the lock across a radio call
//     could deadlock. S3PlusMonitorSource::poll() therefore releases the lock
//     while it drives the radios inside core.tick(), and ReconStart,
//     ReconStop, and SetEarlyWarning run without it.

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace layertime {
namespace twatch_s3plus {

class EventLock {
public:
    // Before any radio starts.
    void begin();
    bool ready() const { return _mutex != nullptr; }
    void take();
    void give();

private:
    SemaphoreHandle_t _mutex = nullptr;
};

class EventLockGuard {
public:
    explicit EventLockGuard(EventLock &lock) : _lock(lock) { _lock.take(); }
    ~EventLockGuard() { _lock.give(); }
    EventLockGuard(const EventLockGuard &) = delete;
    EventLockGuard &operator=(const EventLockGuard &) = delete;

private:
    EventLock &_lock;
};

} // namespace twatch_s3plus
} // namespace layertime
