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

#include "gtest/gtest.h"
#include "mogi_statechart/statechart.hpp"

TEST(BasicTest, chartConfig)
{
  /* create chart */
  EXPECT_THROW(mogi::statechart::Chart::createChart(""), std::runtime_error);

  auto c = mogi::statechart::Chart::createChart("c1");
  ASSERT_NE(c, nullptr) << "Fail to create a chart c1";
  EXPECT_EQ(c->getStateCount(), 2);
  EXPECT_TRUE(c->hasState("initial"));
  EXPECT_TRUE(c->hasState("final"));
  EXPECT_EQ(c->getInitialState()->name(), "initial");
  EXPECT_EQ(c->getFinalState()->name(), "final");


  /* create state */
  EXPECT_THROW(c->createState(""), std::runtime_error);
  auto s = c->createState("s1");
  EXPECT_NE(s, nullptr);
  EXPECT_EQ(c->getStateCount(), 3);
  EXPECT_TRUE(c->hasState("s1"));
  // same state name will return the existing state
  EXPECT_EQ(s, c->createState("s1"));
  EXPECT_EQ(c->getStateCount(), 3);

  /* remove state */
  c->removeState("initial");
  EXPECT_EQ(c->getInitialState()->name(), "initial");
  EXPECT_EQ(c->getStateCount(), 3);
  c->removeState("final");
  EXPECT_EQ(c->getFinalState()->name(), "final");
  EXPECT_EQ(c->getStateCount(), 3);
  c->removeState("s1");
  EXPECT_EQ(c->getStateCount(), 2);

  /* subchart creation */
  auto subc = mogi::statechart::Chart::createChart("c1");
  ASSERT_NE(subc, nullptr) << "Fail to create a subchart c1";
  /* you can add a subchart to a containing chart */
  c->addSubchart(subc);
  EXPECT_EQ(c->getStateCount(), 3);
  subc->createState("sub");
  EXPECT_EQ(c->getStateCount(), 3);
  EXPECT_EQ(subc->getStateCount(), 3);
}

TEST(BasicTest, stateConfig)
{
  auto c = mogi::statechart::Chart::createChart("c1");
  auto s1 = c->createState("s1");
  auto s2 = c->createState("s2");
  EXPECT_EQ(c->getStateCount(), 4);
  EXPECT_EQ(s1->name(), "s1");
  EXPECT_EQ(s2->name(), "s2");

  /* create transition init -> s1 */
  EXPECT_EQ(c->getInitialState()->getTransistionCount(), 0);
  auto t1 = c->getInitialState()->createTransition(s1);
  EXPECT_EQ(c->getInitialState()->getTransistionCount(), 1);
  ASSERT_NE(t1, nullptr);
  EXPECT_EQ(t1->getDst(), s1);

  auto t2 = c->getInitialState()->createTransition(s1);
  EXPECT_EQ(c->getInitialState()->getTransistionCount(), 2);
  ASSERT_NE(t2, nullptr);
  EXPECT_EQ(t2->getDst(), s1);

  /* remove a transition */
  c->getInitialState()->removeTransition(t2);
  EXPECT_EQ(c->getInitialState()->getTransistionCount(), 1);

  /* add a transition s1 -> s2, then remove s2 from the chart
   * the added transition should be removed after purgeExpiredTransitions() */
  EXPECT_EQ(s1->getTransistionCount(), 0);
  auto t3 = s1->createTransition(s2);
  ASSERT_NE(t3, nullptr);
  EXPECT_EQ(t3->getDst(), s2);
  EXPECT_EQ(s1->getTransistionCount(), 1);

  EXPECT_EQ(c->getStateCount(), 4);
  c->removeState(s2);
  EXPECT_EQ(c->getStateCount(), 3);
  s1->purgeExpiredTransitions();
  EXPECT_EQ(s1->getTransistionCount(), 0);

  /* you should not be able to add a transition to a dst that's not
   * contained in the same chart
   */
  auto c2 = mogi::statechart::Chart::createChart("c2");
  EXPECT_THROW(s1->createTransition(c2), std::runtime_error);
  auto c2s1 = c2->createState("s1");
  EXPECT_THROW(s1->createTransition(c2s1), std::runtime_error);

  c->addSubchart(c2);
  auto subt1 = s1->createTransition(c2);
  ASSERT_NE(subt1, nullptr);
  EXPECT_EQ(subt1->getDst(), c2);
  EXPECT_EQ(s1->getTransistionCount(), 1);

  c->removeState(c2);
  EXPECT_EQ(c->getStateCount(), 3);
  s1->purgeExpiredTransitions();
  EXPECT_EQ(s1->getTransistionCount(), 0);
}

TEST(BasicTest, transitionConfig)
{
  auto c = mogi::statechart::Chart::createChart("c1");
  auto s1 = c->createState("s1");
  auto t1 = c->getInitialState()->createTransition(s1);
  ASSERT_NE(t1, nullptr);
  EXPECT_EQ(t1->getDst(), s1);

  EXPECT_EQ(t1->getGuardCount(), 0);
  auto g1 = t1->createGuard([]() {return true;});
  EXPECT_EQ(t1->getGuardCount(), 1);
  t1->removeGuard(g1);
  EXPECT_EQ(t1->getGuardCount(), 0);
  t1->createGuard([]() {return true;});
  t1->createGuard([]() {return true;});
  EXPECT_EQ(t1->getGuardCount(), 2);

  EXPECT_TRUE(g1->isSatisfied());
}

TEST(BasicTest, eventConfig)
{
  auto c = mogi::statechart::Chart::createChart("c1");
  auto s1 = c->createState("s1");
  auto s2 = c->createState("s2");
  auto t1 = c->getInitialState()->createTransition(s1);
  mogi::statechart::Event e1;
  mogi::statechart::Event e2;

  EXPECT_EQ(e1.observerCount(), 0);
  EXPECT_EQ(e2.observerCount(), 0);
  EXPECT_EQ(s1->eventCount(), 0);
  EXPECT_EQ(s2->eventCount(), 0);

  /* add e1 to s1 */
  s1->createEventCallback(e1, [](const mogi::statechart::Event & e) {(void)e;});
  EXPECT_EQ(e1.observerCount(), 1);
  EXPECT_EQ(s1->eventCount(), 1);
  EXPECT_EQ(e2.observerCount(), 0);
  EXPECT_EQ(s2->eventCount(), 0);

  /* double add the same event */
  auto success = s1->createEventCallback(e1, [](const mogi::statechart::Event & e) {(void)e;});
  EXPECT_FALSE(success);
  EXPECT_EQ(e1.observerCount(), 1);
  EXPECT_EQ(s1->eventCount(), 1);
  EXPECT_EQ(e2.observerCount(), 0);
  EXPECT_EQ(s2->eventCount(), 0);

  /* add e2 to s1 */
  success = s1->createEventCallback(e2, [](const mogi::statechart::Event & e) {(void)e;});
  EXPECT_TRUE(success);
  EXPECT_EQ(e1.observerCount(), 1);
  EXPECT_EQ(e2.observerCount(), 1);
  EXPECT_EQ(s1->eventCount(), 2);
  EXPECT_EQ(s2->eventCount(), 0);

  /* add e1 to s2 */
  success = s2->createEventCallback(e1, [](const mogi::statechart::Event & e) {(void)e;});
  EXPECT_TRUE(success);
  EXPECT_EQ(e1.observerCount(), 2);
  EXPECT_EQ(e2.observerCount(), 1);
  EXPECT_EQ(s1->eventCount(), 2);
  EXPECT_EQ(s2->eventCount(), 1);

  /* add e2 to s2 */
  success = s2->createEventCallback(e2, [](const mogi::statechart::Event & e) {(void)e;});
  EXPECT_TRUE(success);
  EXPECT_EQ(e1.observerCount(), 2);
  EXPECT_EQ(e2.observerCount(), 2);
  EXPECT_EQ(s1->eventCount(), 2);
  EXPECT_EQ(s2->eventCount(), 2);

  /* now let's remove e1 from s1 */
  success = s1->removeEventCallback(e1);
  EXPECT_TRUE(success);
  EXPECT_EQ(e1.observerCount(), 1);
  EXPECT_EQ(e2.observerCount(), 2);
  EXPECT_EQ(s1->eventCount(), 1);
  EXPECT_EQ(s2->eventCount(), 2);

  /* double remove should fail */
  success = s1->removeEventCallback(e1);
  EXPECT_FALSE(success);
  EXPECT_EQ(e1.observerCount(), 1);
  EXPECT_EQ(e2.observerCount(), 2);
  EXPECT_EQ(s1->eventCount(), 1);
  EXPECT_EQ(s2->eventCount(), 2);

  /* remove e2 from s1 */
  success = s1->removeEventCallback(e2);
  EXPECT_TRUE(success);
  EXPECT_EQ(e1.observerCount(), 1);
  EXPECT_EQ(e2.observerCount(), 1);
  EXPECT_EQ(s1->eventCount(), 0);
  EXPECT_EQ(s2->eventCount(), 2);

  /* we can also add event to chart */
  EXPECT_EQ(c->eventCount(), 0);
  success = c->createEventCallback(e2, [](const mogi::statechart::Event & e) {(void)e;});
  EXPECT_TRUE(success);
  EXPECT_EQ(e1.observerCount(), 1);
  EXPECT_EQ(e2.observerCount(), 2);
  EXPECT_EQ(s1->eventCount(), 0);
  EXPECT_EQ(s2->eventCount(), 2);
  EXPECT_EQ(c->eventCount(), 1);

  /* add event to transition */
  success = t1->addEvent(e1);
  EXPECT_TRUE(success);
  EXPECT_EQ(e1.observerCount(), 2);
  EXPECT_EQ(t1->eventCount(), 1);

  /* double add */
  success = t1->addEvent(e1);
  EXPECT_FALSE(success);
  EXPECT_EQ(e1.observerCount(), 2);
  EXPECT_EQ(t1->eventCount(), 1);

  /* add e2 */
  success = t1->addEvent(e2);
  EXPECT_TRUE(success);
  EXPECT_EQ(e1.observerCount(), 2);
  EXPECT_EQ(e2.observerCount(), 3);
  EXPECT_EQ(t1->eventCount(), 2);

  /* remove e1 */
  success = t1->removeEvent(e1);
  EXPECT_TRUE(success);
  EXPECT_EQ(e1.observerCount(), 1);
  EXPECT_EQ(e2.observerCount(), 3);
  EXPECT_EQ(t1->eventCount(), 1);

  /* double remove */
  success = t1->removeEvent(e1);
  EXPECT_FALSE(success);
  EXPECT_EQ(e1.observerCount(), 1);
  EXPECT_EQ(e2.observerCount(), 3);
  EXPECT_EQ(t1->eventCount(), 1);
}
