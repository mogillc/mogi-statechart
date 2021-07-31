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

#include <iostream>
#include <memory>
#include <string>
#include "mogi_statechart/statechart.hpp"

using mogi::statechart::Chart;
using mogi::statechart::Event;

/* An example with nested chart setup.
 *
 * In this example, we will create a chart {chart} that has two user states,
 * one is an ordinary state with the name state1, and another chart
 * {subchart} as the other state. We will create transitions from initial
 * to state1, [tSub] from state1 to subchart, and [tFinal] from subchart to final.
 * We will also add a transition [tAgain] from {subchart} back to state1 to
 * simulate a loop
 * We will add a guard <gReady> on [tSub] to indicate we are ready to transit
 * into {subchart} and an event (eFinish) on [tFinal] to indicate we should go to
 * the final state. Also an event (eAgain) on [tAgain] will grant the transition
 * to put us back to state1
 * We add one event (eSub) on the subchart as well.
 *
 * {chart} will look like the following
 *
 *                          { chart }
 *
 *        [tInit]         [tSub]      (eSub)     [tFinal]
 * initial -----> state1 --------->  {subchart}  -------> final
 *                 /\     <gReady>       |       (eFinish)
 *                 |                     |
 *                 |      [tAgain]       |
 *                 +---------------------+
 *                        (eAgain)
 *
 * In side the subchart we will reuse the same chart we created in
 * examples/simple.cpp:
 *
 *                 { subchart }
 *
 *  (e1)      [t1]   (e1,e2,e3)   [t2]
 * initial --------->   [s1]  ----------> final
 *            <g1>             (eT2)<g2>
 *
 * Please refer to examples/simple.cpp for more details.
 *
 * How would this chart run:
 * At startup, we will start with initial state and transit to state1
 * unconditionally, where we will wait for <gReady> to become to true.
 * Once this guard is satisfied, we take [tSub] to the {subchart} and
 * start running the subchart there with the same policies we setup as
 * in examples/simple.cpp. At anypoint, an event (eAgain) will put us
 * back to state1 and if <gReady> is still true we will go back to
 * subchart again but with {subchart} running from its own initial, i.e.
 * {subchart} is reset upon each re-entry. On the other hand, if an event
 * of (eFinish) is trigger while we are in {subchart} state, we will
 * transit to the final state and finish off.
 *
 */

/*
 * A helper class to provide a callback in do state which also
 * counts the number of time the callback has been called
 */
class StateDoCounter
{
public:
  void callback(void)
  {
    std::cout << "<Do> called *" << ++counter << "* time(s)" << std::endl;
  }
  void clearCounter(void)
  {
    counter = 0;
  }

private:
  int counter{};
};

/*
 * A helper class to provide a event callback and print the triggered
 * event's name
 */
class StateEventLogger
{
public:
  void callback(const Event & e)
  {
    std::cout << "(Event):" << e.name() << std::endl;
    lastLoggerEvent = e;
  }

private:
  Event lastLoggerEvent;
};

/*
 * A helper class to serve as a state change observer
 */
class StateChangeObserver
{
public:
  explicit StateChangeObserver(std::shared_ptr<Chart> & c)
  : chart(c) {}

  void printStateName(const std::string & s)
  {
    (void)s;
    std::cout << "[Observer]:" << chart->getCurrentStateNameFull() << std::endl;
  }

private:
  std::shared_ptr<Chart> chart;
};

struct SubChartControl
{
  StateDoCounter s1DoCounter;
  bool g1Flag{false}, g2Flag{false};
  Event e1{"e1"}, e2{"e2"}, e3{"e3"};
  Event eT2{"eT2"};
  StateEventLogger s1EventLogger;

  std::shared_ptr<Chart> createSubChart()
  {
    /* ==== create the chart ==== */
    auto chart = mogi::statechart::Chart::createChart("subchart");
    /* ==== create state s1 ==== */
    auto s1 = chart->createState("s1");
    /* register state callbacks for s1, here */
    s1->setCallbackEntry([]() {std::cout << "<s1 Entry> called!" << std::endl;});
    s1->setCallbackDo(std::bind(&StateDoCounter::callback, &s1DoCounter));
    s1->setCallbackExit([]() {std::cout << "<s1 Exit> called!" << std::endl;});
    /* ==== create transtions t1 and t2 ==== */
    auto t1 = chart->getInitialState()->createTransition(s1);
    /* t2 will be created with a callback */
    auto t2 = s1->createTransition(
      chart->getFinalState(),
      []() {std::cout << "[t2]" << std::endl;});
    /* ==== create guards g1 and g2 for t1 and t2 ==== */
    t1->createGuard([this]() {return this->g1Flag;});
    t2->createGuard([this]() {return this->g2Flag;});
    /* ==== add event e1 to initial state  ==== */
    chart->getInitialState()->createEventCallback(
      e1,
      [](auto event) {
        std::cout << "Event:[Initial]:" << event.name() << std::endl;
      });
    /* ==== add event e1,e2,e3 to s1 ==== */
    s1->createEventCallback(
      e1,
      [](auto event) {
        std::cout << "Event:[s1Lambda]:" << event.name() << std::endl;
      });
    /* e2 and e3 will share one common event logger */
    s1->createEventCallback(
      e2,
      std::bind(
        &StateEventLogger::callback, &s1EventLogger,
        std::placeholders::_1));
    s1->createEventCallback(
      e3,
      std::bind(
        &StateEventLogger::callback, &s1EventLogger,
        std::placeholders::_1));
    t2->addEvent(eT2);

    return chart;
  }
};

int main(void)
{
  /* ==== create chart, state1 as well as the subchart ==== */
  auto chart = Chart::createChart("chart");
  auto state1 = chart->createState("state1");
  SubChartControl subChartControl;
  auto subchart = subChartControl.createSubChart();
  /* add subchart to our chart */
  chart->addSubchart(subchart);

  /* ==== create transtions ==== */
  chart->getInitialState()->createTransition(
    state1,
    []() {std::cout << "[tInit]" << std::endl;});
  auto tSub = state1->createTransition(
    subchart,
    []() {std::cout << "[tSub]" << std::endl;});
  auto tFinal = subchart->createTransition(
    chart->getFinalState(),
    []() {std::cout << "[tFinal]" << std::endl;});
  auto tAgain = subchart->createTransition(
    state1,
    []() {std::cout << "[tAgain]" << std::endl;});

  /* ==== add guards and events ==== */
  bool gReadyFlag{false};
  tSub->createGuard([&gReadyFlag]() {return gReadyFlag;});

  Event eFinish{"eFinish"}, eAgain{"eAgain"};
  tFinal->addEvent(eFinish);
  tAgain->addEvent(eAgain);

  Event eSub{"eSub"};
  subchart->createEventCallback(
    eSub,
    /* let's reuse the one in our subchart */
    std::bind(
      &StateEventLogger::callback, &subChartControl.s1EventLogger,
      std::placeholders::_1));

  /* ==== Finally, a state change observer for bot chart and subchart ==== */
  StateChangeObserver sObs{chart};
  chart->createStateChangeCallback(
    std::bind(
      &StateChangeObserver::printStateName,
      &sObs, std::placeholders::_1));
  subchart->createStateChangeCallback(
    std::bind(
      &StateChangeObserver::printStateName,
      &sObs, std::placeholders::_1));

  std::cout << "===== start =====" << std::endl;
  chart->spinAsync();
  /* wait until we reached state1 */
  while (!state1->isActive()) {}

  std::cout << "------ grant tSub and t1 ------" << std::endl;
  /* grant transition tSub */
  gReadyFlag = true;
  /* grant transition t1 inside subchart as well */
  subChartControl.g1Flag = true;

  /* wait until subchart reaches s1 */
  while (subchart->getCurrentStateName() != "s1") {}
  /* after some delay, fire some events */
  std::this_thread::sleep_for(std::chrono::microseconds(10));
  std::cout << "------ trigger events e1, e2, e3  ------" << std::endl;
  subChartControl.e1.trigger();
  subChartControl.e2.trigger();
  subChartControl.e3.trigger();

  /* revoke guard gReady and g1 */
  gReadyFlag = false;
  subChartControl.g1Flag = false;
  /* after a short delay, let's trigger eAgain to go back state1 */
  std::this_thread::sleep_for(std::chrono::microseconds(10));
  std::cout << "------ trigger eAgain  ------" << std::endl;
  eAgain.trigger();
  while (!state1->isActive()) {}

  /* now re-grant tSub, since subchart g1 is revoked, we should
   * be waiting in subchart's initial state
   */
  std::cout << "------ grant tSub ------" << std::endl;
  gReadyFlag = true;
  while (!subchart->getInitialState()->isActive()) {}

  /* now we want to advance subchart to reach its final state, and we also
   * want the main chart to move to final once the subchart hits its own
   * final state. To do that, we can create yet another state observer for
   * subchart and listens to its state change, upon entrying subchart's final
   * this observer will trigger our event (eFinish) to move the main chart to
   * its final
   *
   * Since we don't support dynamic reconfiguration (yet), we need to stop the
   * chart before doing so (add stateChangeObserver) and restart thereafter
   */

  std::cout << "------ setup tFinal hook ------" << std::endl;
  chart->stop();
  subchart->createStateChangeCallback(
    [&eFinish](auto s) {
      if (s == "final") {
        eFinish.trigger();
      }
    });
  chart->spinAsync();

  /* grant guard g1 and g2 in subchart */
  subChartControl.g1Flag = true;
  subChartControl.g2Flag = true;

  /* some delay later, trigger eT2 and watch both subchart and chart goes
   * to final
   */
  std::this_thread::sleep_for(std::chrono::microseconds(50));
  std::cout << "------ trigger eT2 ------" << std::endl;
  subChartControl.eT2.trigger();

  while (!chart->getFinalState()->isActive()) {}
  chart->stop();
  std::cout << "===== stop =====" << std::endl;
}
