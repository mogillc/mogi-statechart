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

#ifndef MOGI_STATECHART__STATECHART_HPP_
#define MOGI_STATECHART__STATECHART_HPP_

#include <functional>
#include <future>
#include <iostream>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>
#include "mogi_statechart/visibility_control.h"

namespace mogi
{

/**
 * @namespace Mogi::StateChart
 * \brief An implementation of UML style state models.
 *
 * \note This implementation does not support dynamic reconfiguration of the
 * chart, i.e. you will need to stop() the chart to change some configurations
 * like state callbacks and transitions etc. if the chart is running
 * (start()-ed)
 * Future implementation might add this support if there's a high demand
 */
namespace statechart
{

class AbstractState;
class MOGI_STATECHART_PUBLIC State;
class MOGI_STATECHART_PUBLIC Chart;

/*!
 @class Callback
 */
template<typename RetT, typename ... ArgsT>
class Callback
{
public:
  using CallbackT = std::function<RetT(ArgsT... args)>;

  explicit Callback(CallbackT && callback)
  : func_(std::forward<CallbackT>(callback)) {}

  /*!
   \brief User-definable callback method.
   */
  RetT invoke(ArgsT... args) const {return func_(args ...);}

  /*!
   \brief Sets internal function handle
   */
  template<typename FuncT>
  void set(FuncT && func)
  {
    func_ = std::forward<FuncT>(func);
  }

private:
  CallbackT func_;
};

class EventObserver;
/*!
 @class Event
 \brief A representation of an event in UML
 @since 12-05-2020
 */
class MOGI_STATECHART_PUBLIC Event
{
public:
  explicit Event(const std::string & n = "anonymous")
  : name_(n) {}

  /*!
   \brief Returns name of the event
   */
  const std::string & name() const {return name_;}

  /*!
   \brief Triggers the specific event
   */
  void trigger();

  /*!
   \brief Add an event observer to this event
   */
  void addObserver(const std::shared_ptr<EventObserver> & observer);

  /*!
   \brief Removes an event observer from this event's observers list
   */
  void removeObserver(const std::shared_ptr<EventObserver> & observer);

  /*!
   \brief Returns number of observers
   */
  int observerCount() const;

private:
  std::vector<std::weak_ptr<EventObserver>> eventObservers;

  std::string name_;
};
/*!
 @class EventObserver
 \brief An event observer listens to an event
 @since 12-05-2020
 */
class EventObserver : public std::enable_shared_from_this<EventObserver>
{
  friend void Event::trigger();

public:
  virtual ~EventObserver() = default;

protected:
  virtual void notify(const Event &) = 0;

  template<typename Type>
  std::shared_ptr<Type> sharedPtr()
  {
    return std::dynamic_pointer_cast<Type>(shared_from_this());
  }
};

/*!
 @class Guard
 \brief A representation of a guard in UML.
 @since 12-01-2020
 */
class Guard : public Callback<bool>
{
public:
  explicit Guard(Callback::CallbackT && callback)
  : Callback<bool>(std::forward<Callback::CallbackT>(callback)) {}

  /*!
   \brief Guard method, calls provided callback in constructor
   @return Pass along the return value from user provided callback
   */
  bool isSatisfied() {return Callback<bool>::invoke();}
};

/*!
 @class Transition
 \brief The representation and configuration of a UML transition.

 This is only creatable by calling State::createTransition() to help ensure a
 properly connected diagram.  This follows the UML style of transition by having
 configurable guards, events, and an action.  If no guard (future event?) is set
 then this will be treated like a completion transition.
 @since 12-03-2020
 */
class MOGI_STATECHART_PUBLIC Transition : public EventObserver
{ // public EventObserver { // event observer for transition performance.
  friend class Chart;

private:
  const std::weak_ptr<Chart> container;
  const std::weak_ptr<AbstractState> src;
  const std::weak_ptr<AbstractState> dst;

  /*!
   \brief Checks all guards, and returns the ANDed output of all guards.
   @return Returns the ANDed output of all guards.
   */
  bool guardsSatisfied() const;

  /*!
   \brief A transition should on happen when 1. all guards are satisfied and
   2. any events the transition is subscribed to is triggered.
   * This method will call guardsSatisfied() internally.
   @return true if the transition should take place
   */
  bool shouldPerform();

  std::vector<std::shared_ptr<Guard>> guards;
  std::unordered_set<const Event *> events_;
  std::atomic<bool> eventTriggered_{false};

  Callback<void> action_callback_ {[]() {}};

protected:
  /*!
   \brief Called when the transition is being performed.
   */
  void action() {action_callback_.invoke();}
  /*!
   \brief Event notification.
   @param event The event that cause the notification.
   */
  void notify(const Event &) override;

public:
  class Enabler
  {
    friend class statechart::AbstractState;

private:
    Enabler() {}
  };

  template<typename ActionT>
  explicit Transition(
    Enabler, const std::weak_ptr<Chart> & c,
    const std::shared_ptr<AbstractState> & s,
    const std::shared_ptr<AbstractState> & d,
    ActionT && action)
  : container(c), src(s), dst(d),
    action_callback_(std::forward<ActionT>(action)) {}

  /*!
   \brief Appends a Guard to the transition.
   This transition is blocked until all guards have been satisfied.
   @param callback Callback funtion returning the status of the guard
   @return a reference to the appended guard in case it needs to be removed later
   */
  template<typename CallbackT>
  std::shared_ptr<Guard> createGuard(CallbackT && callback)
  {
    auto g = std::make_shared<Guard>(std::forward<CallbackT>(callback));
    guards.push_back(g);
    return g;
  }

  /*!
   \brief Removes an appended guard
   @param g Returned from createGuard()
   */
  void removeGuard(const std::shared_ptr<Guard> & g);

  /*!
   \brief Adds the event that performs this transition when guards have been
   satisfied.
   @param event The event that performs calls transition.
   */
  bool addEvent(Event & event);

  /*!
   \brief Removes an added event
   @param event Previously added event
   */
  bool removeEvent(Event & event);

  /*!
   \brief Number of events this transition is subscribed to
   */
  int eventCount() const {return events_.size();}

  /*!
   \brief Destiny state this transition is pointing to
   */
  std::shared_ptr<AbstractState> getDst() {return dst.lock();}

  /*!
   \brief Number of guards on this transition
   */
  int getGuardCount() const {return guards.size();}

  // ~Transition() { std::cout<<"~Transition()"<<std::endl; }
};

class MOGI_STATECHART_PUBLIC AbstractState : public EventObserver
{
  friend class Chart;
  using EventCallbackT = Callback<void, const Event &>;

public:
  /*!
   \brief Constructor with optional name.
   \param label State name.
   */
  explicit AbstractState(
    const std::string & n,
    const std::shared_ptr<Chart> & c = {})
  : label(n), container(c) {}
  virtual ~AbstractState() = default;

  /*!
   \brief Creates a new Transition to another state with action callback.
   @param dst The State to transition to.
   @param action Action callback, called when transition is triggered
   @return The newly created Transition.
   */
  template<typename ActionT = Callback<void>>
  std::shared_ptr<Transition> createTransition(
    const std::shared_ptr<AbstractState> & dst,
    ActionT action = Callback<void>{[]() {}})
  {
    /* if dst is not contained in the same chart, throw an exception */
    if (container.lock() != dst->container.lock()) {
      throw std::runtime_error(
              dst->name() + " and " + name() + " are not in the same chart");
    }
    auto transition = std::make_shared<Transition>(
      Transition::Enabler{}, container,
      sharedPtr<AbstractState>(), dst,
      std::forward<ActionT>(action));
    outgoingTransitions.insert(transition);
    return transition;
  }

  /*!
   \brief Removes a Transition from this state.
   @param transition The Transition to be removed.
   */
  void removeTransition(const std::shared_ptr<Transition> & transition);

  /*!
   \brief Remove all invalid outgoing transitions due to the removal of its
  destine state
  */
  void purgeExpiredTransitions();

  /*!
   \brief Number of transitions coming out of this state
  */
  int getTransistionCount() const {return outgoingTransitions.size();}

  /*!
   \brief Check if this state is currently active in the chart
   @return true if active
  */
  bool isActive() const;

  /*!
   \brief Subscribe to an event with callback
   @param event Event to be subscribed
   @param callback Callback to be called when the event is triggered
   @return true on success
  */
  template<typename CallbackT>
  bool createEventCallback(Event & event, CallbackT && callback)
  {
    event.addObserver(sharedPtr<EventObserver>());
    return eventCallbacks.emplace(
      std::make_pair(
        &event,
        std::forward<CallbackT>(callback)))
           .second;
  }

  /*!
   \brief Removes a subscribed event
   @param event Event to be removed
   @return true on success
  */
  bool removeEventCallback(Event & event);

  /*!
   \brief Number of events this state is subscribed to
  */
  int eventCount() {return eventCallbacks.size();}

  /*!
   \brief Travers the chart as far as neccessary to retrieve the outmost containing chart
  */
  std::shared_ptr<Chart> outmostContainer();

  /*!
   \brief Get name of the state
   */
  virtual const std::string & name() const {return label;}

protected:
  /*! The state's name.
   */
  std::string label;

  /*!
   \brief Called when the state becomes the current state in the Diagram.
   */
  virtual void actionEntry() = 0;
  /*!
   \brief Called on each call Diagram::process() when this state is current in
   the Diagram.
   */
  virtual void actionDo() = 0;
  /*!
   \brief Called just before transition out of this state, when current in the
   Diagram.
   */
  virtual void actionExit() = 0;

  void notify(const Event &) override;

private:
  std::weak_ptr<Chart> container;
  std::unordered_set<std::shared_ptr<Transition>> outgoingTransitions;
  std::atomic<bool> is_active_{false};
  std::unordered_map<const Event *, EventCallbackT> eventCallbacks;

  void setActive(bool active) {is_active_.store(active);}
};

class Chart final : public AbstractState
{
  friend void Transition::notify(const Event & event);

public:
  using StateChangeCallbackT = Callback<void, const std::string &>;
  /*!
   \brief Creates a new state chart, which includes two automatically generated
   states `Initial` and `Final`
   */
  static std::shared_ptr<Chart> createChart(const std::string & n);
  ~Chart();

  /*!
   \brief create a new state in this chart with name n
   */
  std::shared_ptr<State> createState(const std::string & n);

  /*!
   \brief add another chart as a subchart (represented as a state)
   */
  void addSubchart(const std::shared_ptr<Chart> & s);

  /*!
   \brief remove named state from this chart
   */
  void removeState(const std::string & n);

  /*!
   \brief remove subchart state from this chart
   */
  void removeState(const std::shared_ptr<AbstractState> & s);

  /*!
   \brief test if a state is contained in this chart
   */
  bool hasState(const std::string & n);

  /*!
   \brief test if a subchart state is contained in this chart
   */
  bool hasState(const std::shared_ptr<AbstractState> & s) {return hasState(s->name());}

  /*!
   \brief get auto-generated `Initial` state
   */
  const std::shared_ptr<AbstractState> & getInitialState() {return states_.at("initial");}

  /*!
   \brief get auto-generated `Final` state
   */
  const std::shared_ptr<AbstractState> & getFinalState() {return states_.at("final");}

  /*!
   \brief get active state name
   */
  const std::string getCurrentStateName()
  {
    return currentState.load()->name();
  }

  /*!
   \brief get full qualified name of active state name, with containing
   subchart name as prefix if applicable
   */
  const std::string getCurrentStateNameFull() const;

  /*!
   \brief get number of states contained in this chart
   */
  int getStateCount() const {return states_.size();}

  /*!
   \brief Adds a state change callback on this chart. Upon any state transition
   the registered callback will be called
   @param callback Callback function to be registered
   @return Newly added callback handle, used in removeStateChangeCallback()
   */
  template<typename CallbackT>
  std::shared_ptr<StateChangeCallbackT>
  createStateChangeCallback(CallbackT callback)
  {
    auto c = std::make_shared<StateChangeCallbackT>(std::forward<CallbackT>(callback));
    stateChangeCallbacks.push_back(c);
    return c;
  }

  /*!
   \brief remove a state change callback
   @param callback Callback function to be removed, should have been returned
   by calling createStateChangeCallback() previously
  */
  void removeStateChangeCallback(const std::shared_ptr<StateChangeCallbackT> & c);

  /*!
   \brief start the chart process asyncronously
   this will start a new thread
  */
  void spinAsync();

  /*!
   \brief stop updating the chart
  */
  void stop();

  /*!
   \brief run the state chart forever in the calling thread
  */
  void spin();

  /*!
   \brief run one step
  */
  void spinOnce();

  /*!
   \brief run the chart until it reached the specified state
  */
  void spinToState(const std::string & name);

  /*!
   \brief reset the chart to initial status
   if the state is running asyncronously it will be stopped
   without a restarting
  */
  void reset();

  /*!
   \brief return true is the chart is running
  */
  bool isRunning() {return is_running_;}

  void printStates()
  {
    std::cout << name() << ":[ ";
    for (const auto & s : states_) {
      auto c = dynamic_cast<Chart *>(s.second.get());
      if (c) {
        c->printStates();
      } else {
        std::cout << s.second->name() << " ";
      }
    }
    std::cout << "] ";
  }

protected:
  /*!
   \brief Called when the state becomes the current state in the Diagram.
   */
  void actionEntry() override;
  /*!
   \brief Called on each call Diagram::process() when this state is current in
   the Diagram.
   */
  void actionDo() override;
  /*!
   \brief Called just before transition out of this state, when current in the
   Diagram.
   */
  void actionExit() override;
  /*!
   \brief Called on each occurrence of an Event when this state is current in the
   Diagram.
   */
  // virtual void actionEvent(Event* event) override;

private:
  explicit Chart(const std::string & n = "unknown chart")
  : AbstractState(n) {}

  std::shared_ptr<Chart> getSharedPtr() {return sharedPtr<Chart>();}

  std::unordered_map<std::string, std::shared_ptr<AbstractState>> states_;
  std::atomic<AbstractState *> currentState;
  std::atomic<Transition *> pendingTransition;

  std::vector<std::shared_ptr<StateChangeCallbackT>> stateChangeCallbacks;

  enum class ProcessState {Entry, Do, Exit} processState{ProcessState::Entry};
  void process();
  std::thread process_thread_;
  std::promise<void> exit_signal_;
  std::shared_future<void> future_;

  std::atomic<bool> is_running_ {false};
};

class State : public AbstractState
{
  friend std::shared_ptr<State> Chart::createState(const std::string & n);

private:
  explicit State(
    const std::shared_ptr<Chart> & c,
    const std::string & n = "unknown state")
  : AbstractState(n, c) {}

  Callback<void> entry_callback_ {[]() {}};
  Callback<void> do_callback_ {[]() {}};
  Callback<void> exit_callback_ {[]() {}};

public:
  /*!
   \brief Sets a callback that will be called upon entering this state for the
   first time. This callback will only be called once
  */
  template<typename CallbackT>
  void setCallbackEntry(CallbackT && callback)
  {
    entry_callback_.set(std::forward<CallbackT>(callback));
  }

  /*!
   \brief Sets a callback that will be called while this state is active
   This callback will be called every time on the chart spin*() as long as
   the state is active
  */
  template<typename CallbackT>
  void setCallbackDo(CallbackT && callback)
  {
    do_callback_.set(std::forward<CallbackT>(callback));
  }

  /*!
   \brief Sets a callback that will be called upon exiting this state for the
   first time. This callback will only be called once
  */
  template<typename CallbackT>
  void setCallbackExit(CallbackT && callback)
  {
    exit_callback_.set(std::forward<CallbackT>(callback));
  }

protected:
  /*!
   \brief Called when the state becomes the current state in the Diagram.
   */
  void actionEntry() override {entry_callback_.invoke();}
  /*!
   \brief Called on each call Diagram::process() when this state is current in
   the Diagram.
   */
  void actionDo() override {do_callback_.invoke();}
  /*!
   \brief Called just before transition out of this state, when current in the
   Diagram.
   */
  void actionExit() override {exit_callback_.invoke();}
  /*!
   \brief Called on each occurrence of an Event when this state is current in the
   Diagram.
   */
  // virtual void actionEvent(Event* event) override;
};

}  // namespace statechart
}  // namespace mogi

#endif  // MOGI_STATECHART__STATECHART_HPP_
