// Copyright 2021 Mogi LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <memory>
#include "mogi_statechart/statechart.hpp"

using mogi::statechart::Event;

void Event::trigger()
{
  for (auto const & observer : eventObservers) {
    auto lockedObserver = observer.lock();
    if (lockedObserver) {
      lockedObserver->notify(*this);
    }
  }
}

void Event::addObserver(const std::shared_ptr<EventObserver> & observer)
{
  auto hasObserver = std::find_if(
    eventObservers.begin(),
    eventObservers.end(),
    [&observer](auto ob) {
      return ob.lock() == observer;
    });
  if (hasObserver != eventObservers.end()) {
    return;
  }
  eventObservers.push_back(observer);
}

void Event::removeObserver(const std::shared_ptr<EventObserver> & observer)
{
  eventObservers.erase(
    std::remove_if(
      eventObservers.begin(),
      eventObservers.end(),
      [&observer](auto ob) {
        return ob.lock() == observer;
      }),
    eventObservers.end());
}

int Event::observerCount() const
{
  return eventObservers.size();
}
