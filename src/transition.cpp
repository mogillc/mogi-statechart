/******************************************************************************
 * Copyright 2021 Mogi LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************/

#include "mogi_statechart/statechart.hpp"
#include <algorithm>

using namespace mogi;
using namespace statechart;

bool Transition::addEvent(Event& event)
{
    event.addObserver(sharedPtr<EventObserver>());
    return events_.insert(&event).second;
}

bool Transition::removeEvent(Event& event)
{
    event.removeObserver(sharedPtr<EventObserver>());
    return events_.erase(&event) > 0;
}

bool Transition::shouldPerform()
{
    /* no event, check guardsSatisfied */
    if (events_.size() <= 0) {
        return guardsSatisfied();
    }
    /* else, if event not triggered, return false, otherwise
     * check guardsSatisfied()
     */
    auto wasTriggered = eventTriggered_.exchange(false);
    return wasTriggered ? guardsSatisfied() : false;
}

bool Transition::guardsSatisfied() const
{
    bool shouldPerform = true;

    for (auto & g : guards) {
        shouldPerform &= g->isSatisfied();
    }

    return shouldPerform;
}

void Transition::removeGuard(const std::shared_ptr<Guard>& g)
{
    guards.erase(std::remove(guards.begin(), guards.end(), g),
             guards.end());
}

void Transition::notify(const Event& event) {
    (void)event;
    /* return immediately if src state is not active */
    auto srcState = src.lock();
    if (!srcState || !srcState->isActive()) {
        return;
    }

    /* if the outmost chart is running (async), stop it to make sure
     * we are locked down in a state that's not changing
     * This is neccessary to avoid a rare case where the state may
     * become `!isActive()` right after we set the trigger flag to
     * true, i.e. some other transition is granted at the same time,
     * thus we miss the opportunity to check the trigger flag in the
     * chart's process() function because we moved to another state
     * already.
     *
     * Event trigger on transition should be infrequent thus the overhead
     * should not be significant
     */
    auto mainChart = container.lock()->outmostContainer();
    bool needToPause = mainChart->isRunning() && std::this_thread::get_id() != mainChart->process_thread_.get_id();
    if(needToPause){
        mainChart->stop();
        /* check again make sure the src state is active */
        srcState = src.lock();
        if (!srcState || !srcState->isActive()) {
            mainChart->spinAsync();
            return;
        }
    }
    /* signal that we have received an event */
    eventTriggered_.store(true);

    /* resume if chart was running before */
    if(needToPause){
        mainChart->spinAsync();
    }
}
