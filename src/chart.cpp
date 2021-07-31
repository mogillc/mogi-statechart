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
#include <string>
#include <utility>
#include "mogi_statechart/statechart.hpp"

using mogi::statechart::Chart;
using mogi::statechart::State;

std::shared_ptr<Chart> Chart::createChart(const std::string & n)
{
  /* we don't allow empty name for a chart */
  if (n == "") {
    throw std::runtime_error("Chart name is empty");
  }

  std::shared_ptr<Chart> p(new Chart(n));
  p->currentState.store(p->createState("initial").get());
  p->pendingTransition.store(nullptr);
  p->createState("final");
  return p;
}

Chart::~Chart()
{
  stop();
}

std::shared_ptr<State> Chart::createState(const std::string & n)
{
  /* we don't allow empty name for a state */
  if (n == "") {
    throw std::runtime_error("State name is empty");
  }

  /* return if same state already exist */
  auto hasState = states_.find(n);
  if (hasState != states_.end()) {
    return std::dynamic_pointer_cast<State>(hasState->second);
  }

  std::shared_ptr<State> s(new State(getSharedPtr(), n));
  states_.emplace(std::make_pair(n, s));
  return s;
}

void Chart::addSubchart(const std::shared_ptr<Chart> & s)
{
  s->container = getSharedPtr();
  states_.insert({s->name(), s});
}

void Chart::removeState(const std::string & n)
{
  /* cannot remove initial nor final */
  if (n == "initial" || n == "final") {
    return;
  }

  /* we just remove the states here.
   * all the transitions that have this state as
   * dst will be cleaned out as the dangling
   * weak_ptr is examined in the update loop
   */
  states_.erase(n);
}

void Chart::removeState(const std::shared_ptr<AbstractState> & s)
{
  removeState(s->name());
}

bool Chart::hasState(const std::string & n)
{
  return states_.find(n) != states_.end();
}

void Chart::spinAsync()
{
  if (!container.expired() ) {
    // if this chart is contained in another chart as a subchart,
    // don't run
    return;
  }

  auto wasRunning = is_running_.exchange(true);
  if (!wasRunning) {
    exit_signal_ = {};
    future_ = exit_signal_.get_future();
    process_thread_ = std::thread(
      [this]() -> void {
        std::future_status status;
        do {
          do {
            this->process();
          } while (processState != ProcessState::Do);
          status = this->future_.wait_for(std::chrono::seconds(0));
        } while (status == std::future_status::timeout);
      });
  }
}

void Chart::stop()
{
  auto wasRunning = is_running_.exchange(false);
  if (wasRunning) {
    exit_signal_.set_value();
    process_thread_.join();
  }
}

void Chart::spin()
{
  if (is_running_) {
    return;
  }

  while (true) {
    process();
  }
}

void Chart::spinOnce()
{
  if (is_running_) {
    return;
  }

  /* return after we reached to a Do state
   * i.e. if this is a transient state (Entry/Exit)
   * we should keep processing
   */
  do {
    process();
  } while (processState != ProcessState::Do);
}

void Chart::spinToState(const std::string & name)
{
  if (is_running_) {
    return;
  }

  while (currentState.load()->name() != name) {
    process();
  }
}

void Chart::reset()
{
  stop();
  currentState.load()->setActive(false);
  currentState.store(states_.at("initial").get());
  processState = ProcessState::Entry;
  pendingTransition.store(nullptr);
}

void Chart::process()
{
  switch (processState) {
    case ProcessState::Entry:
      /* there's a pending transition, take it */
      if (pendingTransition.load()) {
        auto d = pendingTransition.load()->dst.lock();
        if (d) {
          currentState.store(d.get());
          pendingTransition.store(nullptr);
        }
      }
      currentState.load()->actionEntry();
      for (const auto & callback : stateChangeCallbacks) {
        callback->invoke(currentState.load()->name());
      }
      processState = ProcessState::Do;
      currentState.load()->setActive(true);
      break;
    case ProcessState::Do:
      currentState.load()->actionDo();
      currentState.load()->purgeExpiredTransitions();
      {
        /*
        auto t = std::find_if(currentState.load()->outgoingTransitions.begin(),
                      currentState.load()->outgoingTransitions.end(),
                      [](auto t){return t->shouldPerform();}
                     );
        if(t != currentState.load()->outgoingTransitions.end()){
            pendingTransition.store((*t).get());
            processState = ProcessState::Exit;
        }
        */
        /* we use the following logic instead of the above one to handle a
         * very rare case where multiple transitions are eligible to take
         * place, for example, one transition might have all its guards
         * satisfied while another transition with a sole event dependency
         * just received the event trigger. In this case we need the make
         * sure we examine every transition by calling its `shouldPerform()`
         * funtion, which will have the side effect of clearing/reseting
         * the triggered event flag.
         *
         * Which transition will eventually take place is arbituary in this
         * case and the implementation choose the last examined one that
         * passes its `shouldPerform()` check
         */
        std::shared_ptr<Transition> t{};
        std::for_each(
          currentState.load()->outgoingTransitions.begin(),
          currentState.load()->outgoingTransitions.end(),
          [&t](auto tt) {
            if (tt->shouldPerform()) {
              t = tt;
            }
          });
        if (t) {
          pendingTransition.store(t.get());
          processState = ProcessState::Exit;
        }
      }
      break;
    case ProcessState::Exit:
      currentState.load()->actionExit();
      pendingTransition.load()->action();
      currentState.load()->setActive(false);
      processState = ProcessState::Entry;
      break;
  }
}

const std::string Chart::getCurrentStateNameFull() const
{
  auto c = dynamic_cast<Chart *>(currentState.load());
  if (c) {
    return std::string{c->name() + ":" + c->getCurrentStateNameFull()};
  } else {
    return currentState.load()->name();
  }
}

void Chart::actionEntry()
{
  reset();
}

void Chart::actionDo()
{
  do {
    process();
  } while (processState != ProcessState::Do);
}

void Chart::actionExit() {}

void Chart::removeStateChangeCallback(const std::shared_ptr<StateChangeCallbackT> & c)
{
  stateChangeCallbacks.erase(
    std::remove(
      stateChangeCallbacks.begin(),
      stateChangeCallbacks.end(),
      c),
    stateChangeCallbacks.end());
}
