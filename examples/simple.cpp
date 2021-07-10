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
#include <iostream>
#include <string>

using namespace mogi;
using namespace statechart;

/* A simple state chart {chart} with one user state s1 (initial and final
 * provided by the chart automatically upon each chart creation),
 * [t1] as the transition from initial to s1, and [t2] from s1 to final,
 * [t1] will have a guard <g1> that's governing its transition.
 * There are three events, (e1, e2, e3), there initial state subscribes to
 * event (e1), and all three events are subscribed by s1 with two
 * callbacks and one event (eT2) subscribed by [t2], which together with
 * guard <g2> will grant the transition t2 to occur
 *
 * s1, t2 also register their callbacks as follows:
 * s1Entry: lambda callback on state entry
 * s1Do: member function callback on state main loop
 * s1Exit: global function callback on state exit
 * t2Cb: lambda callback when transition t2 occurs
 *
 *                 { chart }
 *
 *  (e1)      [t1]   (e1,e2,e3)   [t2]
 * initial --------->   [s1]  ----------> final
 *            <g1>             (eT2)<g2>
 *
 *
 * Additionally, we register a stateChangeObserver sObs
 * on {chart} to get notified on each state transition
 * */

/*
 * Helper function to print current state of a chart
 */
void printCurrentState(const std::shared_ptr<Chart>& c)
{
    std::cout << "{" << c->name() << "} in [" <<
        c->getCurrentStateName() << "]" << std::endl;
}

/*
 * Helper funtion to print a chart's current state
 * and spinOnce() to progress the chart status
 */
void spinAndPrint(const std::shared_ptr<Chart>& c, int& cnt)
{
    std::cout << "------------" << std::endl;
    std::cout << "spin("<<++cnt<<")-> ";
    printCurrentState(c);
    c->spinOnce();
}

/*
 * A helper class to provide a callback in do state which also
 * counts the number of time the callback has been called
 */
class S1DoCounter
{
public:
    void callback(void) {
        std::cout << "<s1 Do> called *" << ++counter << "* time(s)" << std::endl;
    }
    void clearCounter(void) {
        counter = 0;
    }
private:
    int counter;
};

/*
 * A helper class to provide a event callback and print the triggered
 * event's name
 */
class S1EventLogger
{
public:
    void callback(const Event& e) {
        std::cout << "Event:[s1EventLogger]:" << e.name() << std::endl;
        lastLoggerEvent = e;
    }
private:
    Event lastLoggerEvent;
};

/*
 * A helper class to serve as a state change observer
 */
class stateChangeObserver
{
public:
    void printStateName(const std::string& s) {
        std::cout << "[Observer]:" << s << std::endl;
    }
};

/*
 * A global function to serve as a callback for state Exit
 */
void s1ExitCallback(void)
{
    std::cout << "<s1 Exit> called!" << std::endl;
}

int main(void)
{
    /* ==== create the chart ==== */
    auto chart = mogi::statechart::Chart::createChart("chart");

    /* ==== create state s1 ==== */
    auto s1 = chart->createState("s1");

    /* register state callbacks for s1, here
     * we show different ways of providing the callback through
     * 1. lambda function
     * 2. class member function
     * 3. global function
     *
     * These three methods are interchangeable whenever one of them
     * is encountered in all the callback creation related APIs
     */
    s1->setCallbackEntry([](){std::cout << "<s1 Entry> called!" << std::endl;});
    S1DoCounter s1DoCounter;
    s1->setCallbackDo(std::bind(&S1DoCounter::callback, &s1DoCounter));
    s1->setCallbackExit(s1ExitCallback);

    /* ==== create transtions t1 and t2 ==== */
    auto t1 = chart->getInitialState()->createTransition(s1);
    /* t2 will be created with a callback */
    auto t2 = s1->createTransition(chart->getFinalState(),
            [](){std::cout << "<t2 callback> called!" << std::endl;});

    /* ==== create guards g1 and g2 for t1 and t2 ==== */
    /* g1 and g2 will use a simple flag to indicate its status */
    bool g1Flag{false}, g2Flag{false};
    t1->createGuard([&g1Flag](){return g1Flag;});
    t2->createGuard([&g2Flag](){return g2Flag;});

    /* ==== now let's create the events for states initial and s1 ==== */
    Event e1{"e1"}, e2{"e2"}, e3{"e3"};
    /* ==== add event e1 to initial state  ==== */
    chart->getInitialState()->createEventCallback(e1,
            [](auto event){
            std::cout << "Event:[Initial]:" << event.name() << std::endl;});
    /* ==== add event e1,e2,e3 to s1 ==== */
    s1->createEventCallback(e1,
            [](auto event){
            std::cout << "Event:[s1Lambda]:" << event.name() << std::endl;});
    /* e2 and e3 will share one common event logger */
    S1EventLogger s1EventLogger;
    s1->createEventCallback(e2,
            std::bind(&S1EventLogger::callback, &s1EventLogger,
                std::placeholders::_1));
    s1->createEventCallback(e3,
            std::bind(&S1EventLogger::callback, &s1EventLogger,
                std::placeholders::_1));

    /* ==== create event eT2 and add it to t1 ==== */
    Event eT2{"eT2"};
    t2->addEvent(eT2);

    /* ==== Finally, register a state change observer on chart ==== */
    stateChangeObserver sObs;
    chart->createStateChangeCallback(
            std::bind(&stateChangeObserver::printStateName,
                &sObs, std::placeholders::_1));

    /* ==== print chart configuration ==== */
    std::cout << "================ {" << chart->name() << "} configuration ===============" << std::endl;
    // TODO: we might remove this API?
    chart->printStates();
    std::cout << std::endl << std::endl;


    /* =========== Let's get the chart running ============ */
    /* ==== we will first demostrate run the chart syncronously ==== */
    /* we use a local varible to track how many times we called spinOnce()
     * and use the helper function spinAndPrint() to actually print the
     * chart's current state as well as calling spinOnce() for us
     */
    int spinCount{};
    std::cout << "======== Running with spinOnce (syncronously) ========" << std::endl;
    /* spin 1: gets to initial state
     * Notified callbacks:
     *  state observer
     */
    spinAndPrint(chart, spinCount);

    /* spin 2: stays in initial because of guard g1
     * Notified callbacks:
     *   None
     */
    spinAndPrint(chart, spinCount);
    /* Trigger event e1
     * Notified callbacks:
     *   e1 callback on initial state
     */
    e1.trigger();
    /* any other event trigger will not take effect here,
     * thus the following two lines won't make any output
     */
    e3.trigger();
    eT2.trigger();

    /* spin 3: grant g1, transition to state s1
     * Notified callbacks:
     *   state observer
     *   s1 Entry callback
     */
    g1Flag = true;
    spinAndPrint(chart, spinCount);

    /* spin 4: stays in s1
     * Notified callbacks:
     *   s1 Do callback
     */
    spinAndPrint(chart, spinCount);
    /* Trigger event e1, e2, e3
     * Notified callbacks:
     *   e1,e2,e3 callback on s1
     */
    e1.trigger();
    e2.trigger();
    e3.trigger();

    /* spin 5: enable g2, but eT2 did not occur so we stays in s1
     * Notified callbacks:
     *   s1 Do callback
     */
    g2Flag = true;
    spinAndPrint(chart, spinCount);

    /* spin 6: trigger eT2, we will grant transition t2 to final state
     * Notified callbacks:
     *   s1 Do callback (Yes, we will do one last round of Do() before exit)
     *   s1 Exit callback
     *   t2 transition callback
     *   state observer
     */
    eT2.trigger();
    spinAndPrint(chart, spinCount);


    /* ==== we will now demostrate running the chart asyncronously ==== */
    std::cout << std::endl << "======= Running with spinAsync (asyncronously) =======" << std::endl;
    /* let's reset the chart as well all the flags and counters */
    chart->reset();
    s1DoCounter.clearCounter();
    g1Flag = false;
    g2Flag = false;

    /* ==== start the chart in its own thread ==== */
    chart->spinAsync();

    /* ==== after some delay, trigger e1  ==== */
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    e1.trigger();
    /* by the same token, e3 or eT2 will not have any effect */
    e3.trigger();
    eT2.trigger();

    /* ==== after some delay, signal g1 ==== */
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    g1Flag = true;
    /* we can check if we are in state1 yet */
    while (!s1->isActive());
    /* trigger e1, e2, e3 */
    e1.trigger();
    e2.trigger();
    e3.trigger();

    /* ==== enable g2 and trigger eT2 after 500ms later ==== */
    g2Flag = true;
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    eT2.trigger();

    /* ==== wait for the chart to eventally reach final state and stop ==== */
    while(!chart->getFinalState()->isActive());
    chart->stop();
    std::cout << "====================== Stopped =======================" << std::endl;
}
