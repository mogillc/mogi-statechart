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
#include <string>
#include "gtest/gtest.h"
#include "mogi_statechart/statechart.hpp"

using mogi::statechart::Chart;
using mogi::statechart::State;
using mogi::statechart::Transition;

class CallbackTest : public ::testing::Test
{
protected:
  const std::string chartName{"chart1"};
  const std::string stateName{"state"};

  void SetUp() override
  {
    /* chart initial setup will look like the following
     *       /action
     * initial ---> state ---> final
     *
     */
    chart = mogi::statechart::Chart::createChart(chartName);
    state = chart->createState(stateName);

    /* create transition [initial -> state] with action callback */
    tranInitState = chart->getInitialState()->createTransition(
      state,
      std::bind(&CallbackTest::transitionHappend, this));
    /* create transition [state -> final] state */
    tranStateFinal = state->createTransition(chart->getFinalState());
  }

  void TearDown() override {}

  std::shared_ptr<Chart> chart;
  std::shared_ptr<State> state;
  std::shared_ptr<Transition> tranInitState;
  std::shared_ptr<Transition> tranStateFinal;

public:
  void stateChanged(const std::string & name)
  {
    stateChangeFlag = true;
    currentStateName = name;
  }
  bool stateChangeFlag{false};
  std::string currentStateName{""};

  void transitionHappend() {transitionFlag = true;}
  bool transitionFlag{false};

  void resetFlags()
  {
    transitionFlag = false;
    stateChangeFlag = false;
    currentStateName = "";
  }
};

TEST_F(CallbackTest, stateChange)
{
  std::string localCurrentStateName;
  /* add two state change observers, one from CallbackTest, one local closure */
  auto callback1 = chart->createStateChangeCallback(
    std::bind(
      &CallbackTest::stateChanged,
      this, std::placeholders::_1));
  auto callback2 = chart->createStateChangeCallback(
    [&localCurrentStateName](const std::string & name)
    {localCurrentStateName = name;});
  ASSERT_EQ(chart->getCurrentStateName(), "initial");

  ASSERT_FALSE(stateChangeFlag);
  ASSERT_FALSE(transitionFlag);

  /* spin once should get us into init state
   * state change callback should be called but not transition callback
   */
  chart->spinOnce();
  ASSERT_TRUE(stateChangeFlag);
  ASSERT_FALSE(transitionFlag);
  ASSERT_EQ(currentStateName, "initial");

  resetFlags();
  /* spin to the target state
   * state change callback should be called again,
   * as well as transition callback
   */
  chart->spinToState(stateName);
  ASSERT_TRUE(stateChangeFlag);
  ASSERT_TRUE(transitionFlag);
  ASSERT_EQ(localCurrentStateName, stateName);

  resetFlags();
  /* spin chart to the end
   * state change callback should be called again,
   * not the transition callback
   */
  chart->spinToState("final");
  ASSERT_TRUE(stateChangeFlag);
  ASSERT_FALSE(transitionFlag);
  ASSERT_EQ(currentStateName, "final");

  resetFlags();
  /* spin a few more times, this should not trigger any callback */
  for (int i = 0; i < 10; ++i) {
    chart->spinOnce();
  }
  ASSERT_FALSE(stateChangeFlag);
  ASSERT_FALSE(transitionFlag);
  ASSERT_EQ(localCurrentStateName, "final");

  /* reset the flags and the graph, also remove one of the callback */
  resetFlags();
  localCurrentStateName = "";
  chart->reset();
  ASSERT_FALSE(stateChangeFlag);
  ASSERT_FALSE(transitionFlag);
  ASSERT_EQ(currentStateName, "");

  chart->removeStateChangeCallback(callback1);
  /* spin a few times, this should not trigger the removed callback */
  for (int i = 0; i < 100; ++i) {
    chart->spinOnce();
  }
  ASSERT_FALSE(stateChangeFlag);
  ASSERT_TRUE(transitionFlag);
  /* no callback trigger, currentStateName should be empty */
  ASSERT_EQ(currentStateName, "");
  ASSERT_EQ(localCurrentStateName, "final");
}

TEST_F(CallbackTest, stateAction)
{
  bool entryFlag{false}, exitFlag{false};
  int doFlag{0};
  auto entryCallback = [&entryFlag]() {entryFlag = true;};

  state->setCallbackEntry(entryCallback);
  state->setCallbackDo([&doFlag]() {doFlag++;});
  state->setCallbackExit([&exitFlag]() {exitFlag = true;});

  ASSERT_EQ(chart->getCurrentStateName(), "initial");

  /* one spin, put the chart in initial state */
  chart->spinOnce();
  ASSERT_FALSE(entryFlag);
  ASSERT_EQ(doFlag, 0);
  ASSERT_FALSE(exitFlag);

  /* spin to state, this should trigger entry callback */
  chart->spinToState(stateName);
  ASSERT_TRUE(entryFlag);
  ASSERT_EQ(doFlag, 0);
  ASSERT_FALSE(exitFlag);
  ASSERT_EQ(chart->getCurrentStateName(), stateName);

  /* reset entryFlag and spinOnce to final state, this will
   * trigger do callback as well as exit callback
   */
  entryFlag = false;
  chart->spinOnce();
  ASSERT_FALSE(entryFlag);
  ASSERT_NE(doFlag, 0);
  ASSERT_TRUE(exitFlag);
  ASSERT_EQ(chart->getCurrentStateName(), "final");

  /* now reset and remove transition from state -> final
   * let's make sure doAction gets called repeated in the do state
   */
  exitFlag = false;
  doFlag = 0;
  chart->reset();
  state->removeTransition(tranStateFinal);
  ASSERT_FALSE(entryFlag);
  ASSERT_EQ(doFlag, 0);
  ASSERT_FALSE(exitFlag);

  chart->spinToState(stateName);
  ASSERT_TRUE(entryFlag);

  chart->spinOnce();
  ASSERT_EQ(doFlag, 1);

  doFlag = 0;
  for (int i = 0; i < 500; ++i) {
    chart->spinOnce();
  }
  ASSERT_EQ(doFlag, 500);
}

TEST_F(CallbackTest, transitionGuard)
{
  /* add a simple guard on both transitions */
  bool enableInitToState{false}, enableStateToFinal{false};
  tranInitState->createGuard([&enableInitToState]() {return enableInitToState;});
  tranStateFinal->createGuard([&enableStateToFinal]() {return enableStateToFinal;});

  ASSERT_EQ(chart->getCurrentStateName(), "initial");

  /* spin a few times, since the guard is in place, we should stay in initial
   * state
   */
  for (int i = 0; i < 500; ++i) {
    chart->spinOnce();
  }
  ASSERT_EQ(chart->getCurrentStateName(), "initial");

  /* set the first guard to true to grant this transition */
  enableInitToState = true;
  chart->spinOnce();
  ASSERT_EQ(chart->getCurrentStateName(), stateName);

  /* spin a few times, since the guard is in place, we should stay in
   * the same state
   */
  for (int i = 0; i < 500; ++i) {
    chart->spinOnce();
  }
  ASSERT_EQ(chart->getCurrentStateName(), stateName);

  /* set the second guard to true to grant this transition */
  enableStateToFinal = true;
  chart->spinOnce();
  ASSERT_EQ(chart->getCurrentStateName(), "final");

  /* reset the chart, change the two guards to (true,false) */
  chart->reset();
  enableInitToState = true;
  enableStateToFinal = false;

  ASSERT_EQ(chart->getCurrentStateName(), "initial");
  /* spin a few times, since first guard is true and second is false
   * we will staty in the state "stateName"
   */
  for (int i = 0; i < 500; ++i) {
    chart->spinOnce();
  }
  ASSERT_EQ(chart->getCurrentStateName(), stateName);

  /* grant the second guard to transit to final */
  enableStateToFinal = true;
  chart->spinOnce();
  ASSERT_EQ(chart->getCurrentStateName(), "final");
}
