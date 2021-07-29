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

#include "gtest/gtest.h"
#include "mogi_statechart/statechart.hpp"

namespace eventtest
{
    using namespace mogi::statechart;

class RunChartTest
{
    const std::string chartName{"chart"};
    const std::string state1Name{"state1"};
    const std::string state2Name{"state2"};

public:
    virtual const std::string& getState1Name() { return state1Name;}
    virtual const std::string& getState2Name() { return state2Name;}
    const std::string& getChartName() { return chartName;}
    RunChartTest() {
        /* chart initial setup will look like the following
         *
         * initial ---> state1 ---> state2 ---> final
         *
         */
        chart = mogi::statechart::Chart::createChart(chartName);
        state1 = chart->createState(state1Name);
        state2 = chart->createState(state2Name);

        /* create transition [initial -> state1] */
        chart->getInitialState()->createTransition(state1);
        /* create transition [state1 -> state2] */
        tran1To2 = state1->createTransition(state2);
        /* create transition [state2 -> final] state */
        state2->createTransition(chart->getFinalState());

        /* add a default state change callback */
        callback = chart->createStateChangeCallback(
                [this](const std::string& name){
                    this->currentStateName = name;
                });
    }

    std::shared_ptr<Chart> chart;
    std::shared_ptr<AbstractState> state1;
    std::shared_ptr<AbstractState> state2;
    std::shared_ptr<Transition> tran1To2;

    std::shared_ptr<Chart::StateChangeCallbackT> callback;
    std::string currentStateName;
};

class RunSubchartTest : public RunChartTest
{
    const std::string bigchartName{"bigchart"};
    const std::string dummy2Name{"dummy2"};
public:
    /* we will place the subchart as the new state1 */
    virtual const std::string& getState1Name() override { return getChartName();}
    virtual const std::string& getState2Name() override { return dummy2Name;}
    const std::string& getSubState1Name() { return RunChartTest::getState1Name();}
    RunSubchartTest():RunChartTest(){
        /* chart initial setup will look like the following
         *
         * initial ---> state1 ---> state2 ---> final
         *
         */

        /* create the containing chart */
        subchart = mogi::statechart::Chart::createChart(bigchartName);
        subchart->addSubchart(chart);

        /* save state pointer in the subchart */
        subState1 = state1;
        subState2 = state2;
        /* redirect state1 to chart */
        state1 = chart;
        /* create a dummy state2 */
        state2 = subchart->createState(dummy2Name);

        /* create transition [initial -> state1] */
        subchart->getInitialState()->createTransition(state1);
        /* create transition [state1 -> state2] */
        tranSub1To2 = state1->createTransition(state2);
        /* create transition [state2 -> final] state */
        state2->createTransition(subchart->getFinalState());
        /* finally swap the two to reflect their name */
        subchart.swap(chart);
        /* swap the referece to the transisions, such that
         * tran1To2 is the transition from subchart to dummy2
         * and tranSub1To2 is the transition inside the subchart
         * from chart::state1 to chart::state2
         */
        tran1To2.swap(tranSub1To2);
        /* after swap chart will look like
         *                       tranSub1To2
         * inital --> subchart(state1) ---> dummy2(state2) ---> final
         *
         * where subchart is previously configured above
         */

        /* redirect callbacks */
        subchart->removeStateChangeCallback(callback);
        subchart->createStateChangeCallback(
                [this](const std::string& name){
                    this->subchartStateName = name;
                });
        chart->createStateChangeCallback(
                [this](const std::string& name){
                    this->currentStateName = name;
                });

    }

    std::shared_ptr<Chart> subchart;
    std::string subchartStateName;
    std::shared_ptr<Transition> tranSub1To2;
    std::shared_ptr<AbstractState> subState1;
    std::shared_ptr<AbstractState> subState2;
};

template<class T>
class EventTest : public ::testing::Test {
protected:
    T tc;
    const std::string& getName();
};

template<>
const std::string& EventTest<RunChartTest>::getName() { return tc.currentStateName; }
template<>
const std::string& EventTest<RunSubchartTest>::getName() { return tc.currentStateName; }

typedef ::testing::Types<RunChartTest, RunSubchartTest> Implementation;
TYPED_TEST_SUITE(EventTest, Implementation,);

TYPED_TEST(EventTest, spinOnce)
{
    TypeParam c = this->tc;
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.chart->isRunning());

    /* let's add some Event for some state here
     *  eInit         e1         e2        eFinal
     * initial ---> state1 ---> state2 ---> final
     *                     eTran
     */
    Event eInit{"eInit"}, e1{"e1"}, e2{"e2"}, eFinal{"eFinal"};
    Event eTran{"eTran"};
    std::string eventFiredName{};
    int eventFiredCount{};

    c.chart->getInitialState()->createEventCallback(eInit,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    c.state1->createEventCallback(e1,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    c.state2->createEventCallback(e2,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    c.chart->getFinalState()->createEventCallback(eFinal,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    c.tran1To2->addEvent(eTran);

    EXPECT_EQ(eventFiredCount,0);
    EXPECT_EQ(eventFiredName, "");

    /* an initial spin is required to put us in *active* initial state */
    c.chart->spinOnce();
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_TRUE(c.chart->getInitialState()->isActive());

    /* trigger this event */
    eInit.trigger();
    EXPECT_EQ(eventFiredCount,1);
    EXPECT_EQ(eventFiredName, "eInit");

    /* trigger this event again will increment the count */
    eInit.trigger();
    EXPECT_EQ(eventFiredCount,2);
    EXPECT_EQ(eventFiredName, "eInit");

    /* trigger other event shall have no effect */
    eFinal.trigger();
    EXPECT_EQ(eventFiredCount,2);
    EXPECT_EQ(eventFiredName, "eInit");
    e1.trigger();
    EXPECT_EQ(eventFiredCount,2);
    EXPECT_EQ(eventFiredName, "eInit");
    e2.trigger();
    EXPECT_EQ(eventFiredCount,2);
    EXPECT_EQ(eventFiredName, "eInit");
    eTran.trigger();
    eTran.trigger();
    eTran.trigger();
    EXPECT_EQ(eventFiredCount,2);
    EXPECT_EQ(eventFiredName, "eInit");

    /* now one more spin to next state */
    c.chart->spinOnce();
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_FALSE(c.chart->getInitialState()->isActive());
    EXPECT_TRUE(c.state1->isActive());
    /* trigger this event and some other events */
    eInit.trigger();
    EXPECT_EQ(eventFiredCount,2);
    EXPECT_EQ(eventFiredName, "eInit");
    e1.trigger();
    EXPECT_EQ(eventFiredCount,3);
    EXPECT_EQ(eventFiredName, "e1");
    eFinal.trigger();
    EXPECT_EQ(eventFiredCount,3);
    EXPECT_EQ(eventFiredName, "e1");

    /* since we have the event on the transition, we can't move the state */
    for(int i = 0; i < 20; ++i) {
        c.chart->spinOnce();
    }
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_TRUE(c.state1->isActive());

    /* now let's signal the event happended */
    eTran.trigger();

    /* move to next state, and let's remove the event callback for this state*/
    c.chart->spinOnce();
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState2Name());
    EXPECT_TRUE(c.state2->isActive());
    c.state2->removeEventCallback(e2);
    /* trigger some event should have no effect */
    e2.trigger();
    EXPECT_EQ(eventFiredCount,3);
    EXPECT_EQ(eventFiredName, "e1");
    eFinal.trigger();
    EXPECT_EQ(eventFiredCount,3);
    EXPECT_EQ(eventFiredName, "e1");
    eTran.trigger();
    eTran.trigger();
    eTran.trigger();
    EXPECT_EQ(eventFiredCount,3);
    EXPECT_EQ(eventFiredName, "e1");

    /* now let's add both e1 and e2 back to state2 */
    c.state2->createEventCallback(e1,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    c.state2->createEventCallback(e2,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    /* trigger either one should increment the counter */
    e1.trigger();
    EXPECT_EQ(eventFiredCount,4);
    EXPECT_EQ(eventFiredName, "e1");
    e2.trigger();
    EXPECT_EQ(eventFiredCount,5);
    EXPECT_EQ(eventFiredName, "e2");
    e1.trigger();
    EXPECT_EQ(eventFiredCount,6);
    EXPECT_EQ(eventFiredName, "e1");
    /* others won't make effect */
    eInit.trigger();
    eFinal.trigger();
    eTran.trigger();
    EXPECT_EQ(eventFiredCount,6);
    EXPECT_EQ(eventFiredName, "e1");
}

TYPED_TEST(EventTest, spinAsync)
{
    TypeParam c = this->tc;
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.chart->isRunning());

    /* create a guard on the transition state1 -> state2 */
    bool enableTransistion1To2{false};
    c.tran1To2->createGuard([& enableTransistion1To2](){return enableTransistion1To2;});

    /* add the same event callback here */
    Event eInit{"eInit"}, e1{"e1"}, e2{"e2"}, eFinal{"eFinal"};
    Event eTran{"eTran"};
    std::string eventFiredName{};
    int eventFiredCount{};

    c.chart->getInitialState()->createEventCallback(eInit,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    c.state1->createEventCallback(e1,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    c.state2->createEventCallback(e2,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    c.chart->getFinalState()->createEventCallback(eFinal,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    c.tran1To2->addEvent(eTran);

    /* trigger some event here should have no effect */
    e1.trigger();
    e2.trigger();
    eTran.trigger();
    EXPECT_EQ(eventFiredCount,0);
    EXPECT_EQ(eventFiredName, "");

    /* spin the chart asyncronously, it should stop at state1 because
     * of the guard
     */
    c.chart->spinAsync();
    while(!c.state1->isActive());
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_FALSE(c.state2->isActive());

    /* now we fire e1, and we should have event callback
     * associated with e1 in state1 invoked
     */
    e1.trigger();
    EXPECT_EQ(eventFiredCount,1);
    EXPECT_EQ(eventFiredName, "e1");
    e1.trigger();
    EXPECT_EQ(eventFiredCount,2);
    EXPECT_EQ(eventFiredName, "e1");
    /* other event should not matter */
    e2.trigger();
    EXPECT_EQ(eventFiredCount,2);
    EXPECT_EQ(eventFiredName, "e1");
    eInit.trigger();
    EXPECT_EQ(eventFiredCount,2);
    EXPECT_EQ(eventFiredName, "e1");

    /* we can also add e2 to state1 to change its behavior */
    c.state1->createEventCallback(e2,
            [&eventFiredName, &eventFiredCount](auto e)
                {eventFiredName = e.name();
                 eventFiredCount++;});
    /* now it should take effect */
    e2.trigger();
    EXPECT_EQ(eventFiredCount,3);
    EXPECT_EQ(eventFiredName, "e2");
    e1.trigger();
    EXPECT_EQ(eventFiredCount,4);
    EXPECT_EQ(eventFiredName, "e1");

    /* reset the flags */
    eventFiredCount = 0;
    eventFiredName = "";

    /* since we have an event AND a guard on the transition, it's
     * bounded by both conditions
     */
    /* trigger the event alone won't enable the transition */
    eTran.trigger();
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_TRUE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());
    eTran.trigger();
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_TRUE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* on the other hand, simply enable the guard won't make
     * the transition occur either
     */
    enableTransistion1To2 = true;
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_TRUE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* you can still trigger events on the state */
    e1.trigger();
    EXPECT_EQ(eventFiredCount,1);
    EXPECT_EQ(eventFiredName, "e1");

    /* with the guard enabled, an event on the transition will
     * grant the transition and move us to state2
     */
    eTran.trigger();
    while(!c.state2->isActive());
    EXPECT_NE(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_FALSE(c.state1->isActive());

    /* being not in state1, e1 will not take effect */
    e1.trigger();
    EXPECT_EQ(eventFiredCount,1);
    EXPECT_EQ(eventFiredName, "e1");

    /* stop the chart */
    c.chart->stop();
}

class EventLogger
{
public:
    std::string eventFiredName{};
    int eventFiredCount{};
    void eventCallback(const Event& e) {
        eventFiredName = e.name();
        eventFiredCount++;
    }
};

class RunSubchartWithEventTest: public RunSubchartTest, public ::testing::Test
{
public:
    void SetUp() override {
        /* create a guard in main chart from subchart(state1) to state2 */
        tran1To2->createGuard([this](){return this->enableTransistion1To2;});
        /* create a guard inside subchart from chart::state1 to chart::state2 */
        tranSub1To2->createGuard([this](){return this->enableTransistionSub1To2;});
        /* also add an event on each transition */
        tran1To2->addEvent(eTran12);
        tranSub1To2->addEvent(eTranSub12);

        /* add event callbacks in main chart */
        chart->getInitialState()->createEventCallback(eInit,
                std::bind(&EventLogger::eventCallback, &eventLogger,
                    std::placeholders::_1));

        state1->createEventCallback(eState1,
                std::bind(&EventLogger::eventCallback, &eventLogger,
                    std::placeholders::_1));

        state2->createEventCallback(eState2,
                std::bind(&EventLogger::eventCallback, &eventLogger,
                    std::placeholders::_1));

        chart->getFinalState()->createEventCallback(eFinal,
                std::bind(&EventLogger::eventCallback, &eventLogger,
                    std::placeholders::_1));

        /* add event callbacks in sub chart */
        subchart->getInitialState()->createEventCallback(eSubInit,
                std::bind(&EventLogger::eventCallback, &eventLogger,
                    std::placeholders::_1));

        subState1->createEventCallback(eSubState1,
                std::bind(&EventLogger::eventCallback, &eventLogger,
                    std::placeholders::_1));

        subState2->createEventCallback(eSubState2,
                std::bind(&EventLogger::eventCallback, &eventLogger,
                    std::placeholders::_1));

        subchart->getFinalState()->createEventCallback(eSubFinal,
                std::bind(&EventLogger::eventCallback, &eventLogger,
                    std::placeholders::_1));
    }

    bool enableTransistion1To2{false};
    bool enableTransistionSub1To2{false};
    /* use a common logger for all event callbacks */
    EventLogger eventLogger;
    Event eInit{"eInit"}, eState1{"eState1"}, eState2{"eState2"},
          eFinal{"eFinal"}, eSubInit{"eSubInit"}, eSubState1{"eSubState1"},
          eSubState2{"eSubState2"}, eSubFinal{"eSubFinal"};
    /* add event for transisions */
    Event eTran12{"eTran12"}, eTranSub12{"eTranSub12"};
};

TEST_F(RunSubchartWithEventTest, syncRun)
{
    /* kick off the chart */
    chart->spinOnce();
    EXPECT_EQ(chart->getCurrentStateName(), "initial");
    EXPECT_TRUE(chart->getInitialState()->isActive());
    EXPECT_FALSE(subchart->isActive());
    EXPECT_EQ(eventLogger.eventFiredName, "");
    EXPECT_EQ(eventLogger.eventFiredCount, 0);

    /* another spin to main state1(subchart) */
    chart->spinOnce();
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_TRUE(subchart->isActive());
    EXPECT_FALSE(subchart->getInitialState()->isActive());

    /* another spin to kick off subchart */
    chart->spinOnce();
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_TRUE(subchart->isActive());
    EXPECT_TRUE(subchart->getInitialState()->isActive());

    /* fire subchart initial event */
    eSubInit.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eSubInit");
    EXPECT_EQ(eventLogger.eventFiredCount, 1);
    eSubInit.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eSubInit");
    EXPECT_EQ(eventLogger.eventFiredCount, 2);

    /* one more spin to move subchart to its state1 */
    chart->spinOnce();
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_TRUE(subchart->isActive());
    EXPECT_FALSE(subchart->getInitialState()->isActive());
    EXPECT_TRUE(subState1->isActive());

    /* fire other event wouldn't make effect */
    eInit.trigger();
    eSubFinal.trigger();
    eTran12.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eSubInit");
    EXPECT_EQ(eventLogger.eventFiredCount, 2);

    /* fire subState1 event will have effect */
    eSubState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eSubState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 3);
    eSubState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eSubState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 4);
    /* fire main state1 event will also take effect */
    eState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 5);

    /* becauase the guard on the transition, an event alone
     * on the transition won't take us to next state
     */
    eTranSub12.trigger();
    for(int i = 0; i < 20; ++i){
        chart->spinOnce();
    }
    EXPECT_TRUE(subState1->isActive());
    EXPECT_EQ(subchart->getCurrentStateName(), getSubState1Name());

    /* grant subchart transition */
    enableTransistionSub1To2 = true;
    /* the transition won't happend until next event on the transition */
    for(int i = 0; i < 20; ++i){
        chart->spinOnce();
    }
    EXPECT_TRUE(subState1->isActive());
    EXPECT_EQ(subchart->getCurrentStateName(), getSubState1Name());

    /* to trigger the transition, we trigger the event */
    eTranSub12.trigger();
    chart->spinOnce();
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_TRUE(subchart->isActive());
    EXPECT_FALSE(subState1->isActive());
    EXPECT_TRUE(subState2->isActive());
    /* fire main state1 event will take effect */
    eState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 6);
    /* but not substate1 event */
    eSubState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 6);
    /* subState2 event will take effect instead */
    eSubState2.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eSubState2");
    EXPECT_EQ(eventLogger.eventFiredCount, 7);
    /* other events will not have effect */
    eInit.trigger();
    eFinal.trigger();
    eSubInit.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eSubState2");
    EXPECT_EQ(eventLogger.eventFiredCount, 7);

    /* becauase the guard on the transition, an event alone
     * on the transition won't take us to next state
     * in the main chart
     */
    eTran12.trigger();
    for(int i = 0; i < 20; ++i){
        chart->spinOnce();
    }
    EXPECT_TRUE(state1->isActive());
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());

    /* grant transition in main chart */
    enableTransistion1To2 = true;
    /* the transition won't happend until next event on the transition */
    for(int i = 0; i < 20; ++i){
        chart->spinOnce();
    }
    EXPECT_TRUE(state1->isActive());
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());

    /* to trigger the transition, we trigger the event */
    eTran12.trigger();
    chart->spinOnce();
    EXPECT_EQ(chart->getCurrentStateName(), getState2Name());
    EXPECT_FALSE(state1->isActive());
    EXPECT_TRUE(state2->isActive());
    EXPECT_FALSE(subchart->isActive());
    EXPECT_FALSE(subState2->isActive());
    EXPECT_FALSE(subchart->getFinalState()->isActive());
    /* event 2 will take effect now */
    eState2.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState2");
    EXPECT_EQ(eventLogger.eventFiredCount, 8);
}

TEST_F(RunSubchartWithEventTest, asyncRun)
{
    /* start async run */
    chart->spinAsync();
    /* wait till we hit substate1 */
    while(!subState1->isActive());
    EXPECT_EQ(subchart->getCurrentStateName(), getSubState1Name());
    EXPECT_TRUE(chart->isRunning());
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_TRUE(subchart->isActive());
    EXPECT_EQ(subchart->getCurrentStateName(), getSubState1Name());

    /* at this moment we should have both state1 and substate1 events
     * effective but not the others
     */
    EXPECT_EQ(eventLogger.eventFiredName, "");
    EXPECT_EQ(eventLogger.eventFiredCount, 0);
    eInit.trigger();
    eFinal.trigger();
    eState2.trigger();
    eSubInit.trigger();
    eSubFinal.trigger();
    eSubState2.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "");
    EXPECT_EQ(eventLogger.eventFiredCount, 0);

    eState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 1);
    eState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 2);
    eSubState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eSubState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 3);
    eSubState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eSubState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 4);
    eState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 5);

    /* transition guards are false so event on transitions won't
     * make effect either
     */
    eTran12.trigger();
    eTranSub12.trigger();
    eTran12.trigger();
    eTranSub12.trigger();
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_TRUE(subchart->isActive());
    EXPECT_EQ(subchart->getCurrentStateName(), getSubState1Name());

    /* grant subchart transition */
    enableTransistionSub1To2 = true;
    eTranSub12.trigger();
    while(!subState2->isActive());
    EXPECT_NE(subchart->getCurrentStateName(), getSubState1Name());
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_TRUE(subchart->isActive());
    EXPECT_FALSE(subState1->isActive());
    /* fire main state1 event will take effect */
    eState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 6);
    /* but not substate1 event */
    eSubState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 6);
    /* fire main state1 event will take effect */
    eState1.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 7);
    /* other events will not have effect */
    eInit.trigger();
    eFinal.trigger();
    eSubInit.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eState1");
    EXPECT_EQ(eventLogger.eventFiredCount, 7);

    /* grant transition in main chart */
    enableTransistion1To2 = true;
    eTran12.trigger();
    while(!state2->isActive());
    EXPECT_NE(chart->getCurrentStateName(), getState1Name());
    EXPECT_FALSE(state1->isActive());
    EXPECT_FALSE(subchart->isActive());

    /* wait till the end */
    while(!chart->getFinalState()->isActive());
    eFinal.trigger();
    EXPECT_EQ(eventLogger.eventFiredName, "eFinal");
    EXPECT_EQ(eventLogger.eventFiredCount, 8);
}

}
