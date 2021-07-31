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

#include <memory>
#include "mogi_statechart/statechart.hpp"

using mogi::statechart::Chart;
using mogi::statechart::AbstractState;

void AbstractState::removeTransition(const std::shared_ptr<Transition> & transition)
{
  outgoingTransitions.erase(transition);
}

void AbstractState::purgeExpiredTransitions()
{
  /* erase_if is only available since C++20
   * thus we are doing a manual loop here
   */
  for (auto it = outgoingTransitions.begin(), it_end = outgoingTransitions.end();
    it != it_end; )
  {
    auto dst = (*it)->getDst();
    if (dst == nullptr || !container.lock()->hasState(dst)) {
      it = outgoingTransitions.erase(it);
    } else {
      ++it;
    }
  }
}

bool AbstractState::isActive() const
{
  auto c = container.lock();
  /* container is empty, we must the be outmost chart
   * In this case, return true anyways
   */
  if (!c) {
    return true;
  }
  /* now let's check if our container is the outmost
   * chart. i.e. if we are a first class state
   */
  auto cc = c->container.lock();
  if (!cc) {
    /* we are first class, return our own status */
    return is_active_.load();
  } else {
    /* there's more level up, we want both
     * our container and ourselves to be
     * active
     */
    return is_active_.load() && c->isActive();
  }
}

bool AbstractState::removeEventCallback(Event & event)
{
  event.removeObserver(sharedPtr<EventObserver>());
  return eventCallbacks.erase(&event) > 0;
}

void AbstractState::notify(const Event & event)
{
  if (isActive()) {
    auto callbackIt = eventCallbacks.find(&event);
    if (callbackIt != eventCallbacks.end()) {
      callbackIt->second.invoke(event);
    }
  }
}
std::shared_ptr<Chart> AbstractState::outmostContainer()
{
  auto mainChart = container.lock();
  if (!mainChart) {
    return sharedPtr<Chart>();
  }
  while (mainChart->container.lock()) {
    mainChart = mainChart->container.lock();
  }
  return mainChart;
}
