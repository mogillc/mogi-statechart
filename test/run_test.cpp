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

namespace runtest
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
class RunTest : public ::testing::Test {
protected:
    T tc;
    const std::string& getName();
};

template<>
const std::string& RunTest<RunChartTest>::getName() { return tc.currentStateName; }
template<>
const std::string& RunTest<RunSubchartTest>::getName() { return tc.currentStateName; }

typedef ::testing::Types<RunChartTest, RunSubchartTest> Implementation;
TYPED_TEST_CASE(RunTest, Implementation);

TYPED_TEST(RunTest, spinOnce)
{
    TypeParam c = this->tc;
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.chart->isRunning());

    /* first spin will kick off the c.chart */
    c.chart->spinOnce();
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_EQ(this->getName(), "initial");
    EXPECT_FALSE(c.state1->isActive());

    /* another spin brings us to the first state */
    c.chart->spinOnce();
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_EQ(this->getName(), c.getState1Name());
    EXPECT_TRUE(c.state1->isActive());

    /* another spin brings us to the second state */
    c.chart->spinOnce();
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState2Name());
    EXPECT_EQ(this->getName(), c.getState2Name());
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_TRUE(c.state2->isActive());

    /* another spin brings us to the final state */
    c.chart->spinOnce();
    EXPECT_EQ(c.chart->getCurrentStateName(), "final");
    EXPECT_EQ(this->getName(), "final");
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* another spin keeps us in the final state */
    c.chart->spinOnce();
    EXPECT_EQ(c.chart->getCurrentStateName(), "final");
    EXPECT_EQ(this->getName(), "final");

    /* reset chart */
    c.chart->reset();
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());
}

TYPED_TEST(RunTest, spinToState)
{
    TypeParam c = this->tc;
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.chart->isRunning());

    /* let's spin to state1 directly */
    c.chart->spinToState(c.getState1Name());
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_EQ(this->getName(), c.getState1Name());
    EXPECT_TRUE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* do it a few more times, we should stay in the same state */
    for (int i = 0; i < 10; ++i){
        c.chart->spinToState(c.getState1Name());
        EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
        EXPECT_EQ(this->getName(), c.getState1Name());
    }
    EXPECT_TRUE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* reset chart */
    c.chart->reset();
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* a few more time spin to final */
    for (int i = 0; i < 10; ++i){
        c.chart->spinToState("final");
        EXPECT_EQ(c.chart->getCurrentStateName(), "final");
        EXPECT_EQ(this->getName(), "final");
    }
    EXPECT_EQ(c.chart->getCurrentStateName(), "final");
    EXPECT_EQ(this->getName(), "final");
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* reset chart */
    c.chart->reset();
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());
}

TYPED_TEST(RunTest, spinAsync)
{
    TypeParam c = this->tc;
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.chart->isRunning());

    c.chart->spinAsync();
    EXPECT_TRUE(c.chart->isRunning());

    while(c.chart->getCurrentStateName() != "final");
    c.chart->stop();
    EXPECT_EQ(c.chart->getCurrentStateName(), "final");
    EXPECT_FALSE(c.chart->isRunning());
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* spin again, we should still be in the final state */
    c.chart->spinAsync();
    EXPECT_TRUE(c.chart->isRunning());
    c.chart->stop();
    EXPECT_EQ(c.chart->getCurrentStateName(), "final");
    EXPECT_FALSE(c.chart->isRunning());
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* reset chart */
    c.chart->reset();
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());
}

TYPED_TEST(RunTest, spinWithGuard)
{
    TypeParam c = this->tc;
    EXPECT_EQ(c.chart->getCurrentStateName(), "initial");
    EXPECT_FALSE(c.chart->isRunning());

    /* create a guard on the transition */
    bool enableTransistion1To2{false};
    c.tran1To2->createGuard([& enableTransistion1To2](){return enableTransistion1To2;});

    /* do a few spins, we should stay in the first state because the blockage
     * of the guard
     */
    for (int i = 0; i < 10; ++i){
        c.chart->spinOnce();
    }
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState1Name());
    EXPECT_TRUE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());

    /* grant the transition */
    enableTransistion1To2 = true;
    c.chart->spinOnce();
    EXPECT_EQ(c.chart->getCurrentStateName(), c.getState2Name());
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_TRUE(c.state2->isActive());

    /* a few more spins takes us to the final */
    for (int i = 0; i < 10; ++i){
        c.chart->spinOnce();
    }
    EXPECT_EQ(c.chart->getCurrentStateName(), "final");
    EXPECT_FALSE(c.state1->isActive());
    EXPECT_FALSE(c.state2->isActive());
}

class RunSubchartWithGuardTest: public RunSubchartTest, public ::testing::Test {};

TEST_F(RunSubchartWithGuardTest, syncRun)
{
    /* create a guard in main chart from subchart(state1) to state2 */
    bool enableTransistion1To2{false};
    tran1To2->createGuard([& enableTransistion1To2](){return enableTransistion1To2;});
    /* create a guard inside subchart from chart::state1 to chart::state2 */
    bool enableTransistionSub1To2{false};
    tranSub1To2->createGuard([& enableTransistionSub1To2](){return enableTransistionSub1To2;});

    ASSERT_EQ(chart->getCurrentStateName(), "initial");
    ASSERT_FALSE(chart->isRunning());
    EXPECT_FALSE(state1->isActive());
    EXPECT_FALSE(state2->isActive());
    EXPECT_FALSE(subState1->isActive());
    EXPECT_FALSE(subState2->isActive());

    /* do a few spins, we should stay in the first state because the blockage
     * of the guard
     */
    for (int i = 0; i < 100; ++i){
        chart->spinOnce();
    }
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_EQ(currentStateName, getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_FALSE(state2->isActive());
    /* moreover, inside the subchart we should stay in the chart::state1 state
     */
    EXPECT_EQ(subchart->getCurrentStateName(), getSubState1Name());
    EXPECT_TRUE(subState1->isActive());
    EXPECT_FALSE(subState2->isActive());
    /* also check stateChange callback is called in the subchart */
    EXPECT_EQ(subchartStateName, getSubState1Name());

    /* and the full name of current state will have the prefix of the subchart
     */
    EXPECT_EQ(chart->getCurrentStateNameFull(), getState1Name()+":"+getSubState1Name());

    /* now grant the transition inside the subchart only */
    enableTransistionSub1To2 = true;
    for (int i = 0; i < 100; ++i){
        chart->spinOnce();
    }
    /* main chart should stay where it was */
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_FALSE(state2->isActive());
    /* inside the subchart we should move on to the final state */
    EXPECT_EQ(subchart->getCurrentStateName(), "final");
    EXPECT_FALSE(subState1->isActive());
    EXPECT_FALSE(subState2->isActive());
    /* also check the callback is called */
    EXPECT_EQ(subchartStateName, "final");

    /* now let's reset the main chart */
    chart->reset();
    ASSERT_EQ(chart->getCurrentStateName(), "initial");
    ASSERT_FALSE(chart->isRunning());
    EXPECT_FALSE(state1->isActive());
    EXPECT_FALSE(state2->isActive());
    /* subchart pointer is not reset by this
     * it will get reset upon reentry into
     * initial state
     */
    EXPECT_EQ(subchart->getCurrentStateName(), "final");
    /* however, the active flag should have been deasserted because
     * the containing subchart is not active anymore
     */
    EXPECT_FALSE(subState1->isActive());
    EXPECT_FALSE(subState2->isActive());
    /* let's make subchart stop at state1 again to verify */
    enableTransistionSub1To2 = false;
    for (int i = 0; i < 100; ++i){
        chart->spinOnce();
    }
    EXPECT_EQ(chart->getCurrentStateName(), getState1Name());
    EXPECT_TRUE(state1->isActive());
    EXPECT_FALSE(state2->isActive());
    /* subchart gets into state1, meaning it was reset upon entry */
    EXPECT_EQ(subchart->getCurrentStateName(), getSubState1Name());
    EXPECT_TRUE(subState1->isActive());
    EXPECT_FALSE(subState2->isActive());

    /* now let's grant the main chart transition */
    enableTransistion1To2 = true;
    for (int i = 0; i < 100; ++i){
        chart->spinOnce();
    }
    /* main chart should go to final state */
    EXPECT_EQ(chart->getCurrentStateName(), "final");
    EXPECT_FALSE(state1->isActive());
    EXPECT_FALSE(state2->isActive());
    /* subchart gets stays in state1 because of the guard */
    EXPECT_EQ(subchart->getCurrentStateName(), getSubState1Name());
    /* and likewise, it looses its active flag */
    EXPECT_FALSE(subState1->isActive());
    EXPECT_FALSE(subState2->isActive());

    /* at this point even if we grant the subchart transition it
     * should not take place
     */
    enableTransistionSub1To2 = true;
    for (int i = 0; i < 100; ++i){
        chart->spinOnce();
    }
    EXPECT_EQ(chart->getCurrentStateName(), "final");
    EXPECT_EQ(subchart->getCurrentStateName(), getSubState1Name());
    EXPECT_FALSE(subState1->isActive());
    EXPECT_FALSE(subState2->isActive());
}

}
