# mogi_statechart

A UML state chart library.

* [Intro](#intro)
* [Build](#build)
* [Usage Example](#usage-example)
  + [Example setup](#example-setup)
    - [States](#states)
    - [Transitions and Guards](#transitions-and-guards)
    - [Events](#events)
  + [Chart creation](#chart-creation)
    - [States](#states-1)
    - [Transitions and Guards](#transitions-and-guards-1)
    - [Events](#events-1)
  + [Run the chart](#run-the-chart)
    - [Syncronous (Blocking)](#syncronous--blocking-)
    - [Asyncronous (NonBlocking)](#asyncronous--nonblocking-)
  + [Trigger event](#trigger-event)
* [Appendix](#appendix)
  + [Thread model](#thread-model)
      * [Limitations on Async implementation](#limitations-on-async-implementation)
  + [Exceptions](#exceptions)

## Intro
This library provides APIs for creating a UML style state chart
(see [UML state machine](https://en.wikipedia.org/wiki/UML_state_machine)).
It provides basic building blocks for a state chart including:

* State
* Event
* Guard
* Action

It also has the following features:

* Automatically generated *initial* and *final* state for each chart
* Support for composite State (i.e. using a state chart (*subchart*) as a State
  inside another chart)
* Support observers on events
* Support for callbacks on state transitions (entry, do, exit)
* Support for both blocking and nonblocking state chart progression

## Build
```bash
cmake -S . -B build
cmake --build build
# (optionally) run unit test
cd build && ctest
```

## Usage Example
You can find more usage examples in the `examples` folder

We will demostrate a simple state chart here to illustrate the usages of this
library.
The state chart is a slightly simplified version of `examples/simple.cpp` and
it look like following:

### Example setup

```
                                     e1
┌───────────┐                      ┌──────┐                    ┌─────────┐
│           │                      │      │     e2             │         │
│  Initial  ├─────────────────────►│  s1  ├───────────────────►│ Final   │
│           │    t1:g1             │      │     t2:  /action() │         │
└───────────┘                      └──────┘                    └─────────┘
                                   entry:fn_en()
                                   do:fn_do()
                                   exit:fn_ex()
```
#### States
* Two automatically generated states: `Inital` and `Final`
* One user state `s1`, with following state actions:
  * entry: `fn_en()`
  * do: `fn_do()`
  * exit: `fn_ex()`

#### Transitions and Guards
* `t1`: Transition from `Inital` to `s1`
  * Guarded by guard `g1`
* `t2`: Transition from `s1` to `Final`
  * Guarded by event `e2`
  * Also performs action function `action()` upon transition

#### Events
* `e1`: Subscribed by state `s1`
* `e2`: Subscribed by transition `t2`

### Chart creation
We now demonstrate how to create the example state chart above

#### States
* create a containing `Chart`
```cpp
auto chart = Chart::createChart("chart");
```
This will automatically generate `Initial` and `Final` states

*Note* These two states can be retrieved by calling `getInitialState()` and
`getFinalState()` methods on the containing chart

* create user state `s1`
```cpp
auto s1 = chart->createState("s1");
```
Set up state actions respectively
```cpp
s1->setCallbackEntry(fn_en);
s1->setCallbackDo(fn_do);
s1->setCallbackExit(fn_ex);
```

#### Transitions and Guards
* create transtions t1, without action
```cpp
auto t1 = chart->getInitialState()->createTransition(s1);
```
* create transtions t2, with callback action `action()`
```cpp
auto t2 = s1->createTransition(chart->getFinalState(), action);
```

* create guards `g1` and `g2`
Guards takes the form of a functor that returns `true` or `false`, here we
specify `g1` as a lambda function that always returns `true` and `g2` a lambda
that checks a local flag
```cpp
bool g2_flag;
t1->createGuard([](){return true});
t2->createGuard([&g2_flag](){return g2_flag;});
```

#### Events
* Instantiate events `e1` and `e2`
```cpp
Event e1{"e1"}, e2{"e2"};
```

* add event `e1` to state `s1`, with a trivial callback to print a message on
std out
```cpp
s1->createEventCallback(e1,
        [](auto event){
        std::cout << "Event:[in s1]:" << event.name() << std::endl;});
```

* add event `e2` to transition `t2`
```cpp
t2->addEvent(e2);
```

At this point our state chart is fully constructed according to our diagram.

You can also optionally add a state change callback through
`createStateChangeCallback(*Functor*)` method providing a Funtor that will
be called each time a state transition occurs.

### Run the chart
A state chart can run in either syncronously (in the calling thread) or
asyncronously (in another (spawned) thread). A detailed description can be found
in the [Appendix Section](#thread-model)

#### Syncronous (Blocking)
As a simple example, we choose to run the chart in a syncronous (blocking)
manner as:
```cpp
chart->spin();
```
#### Asyncronous (NonBlocking)
An asyncronous (nonblocking) API
```cpp
chart->spinAsync();
```
will spawn up another thread for the chart to progress. To stop this progress
(and thus stop this spawned thread) one needs to call
```cpp
chart->stop();
```
respectively.

### Trigger event
Contiuing on the previous example, we can also trigger events, for example:
```cpp
e1.trigger();
...
e2.trigger();
```
will notify event listeners on the corresponding events and call their callbacks
(e.g. Functor supplied with the call `s1->createStateChangeCallback()`) or
grant a transition (e.g. transition `t2` if current state is `s1`)

## Appendix
### Thread model
The startchart can be invoked both syncronously and asyncronously
* Sync call
    * `spin()` will run the state chart indefinitely in the calling thread,
       blocking the calling thread thereafter.
    * `spinOnce()` will take one step in the state chart logic and return.
       CallbackDo() of this state("A") will be called;
       additionally, if a transition can be taken it will be,
       and CallbackExit() of this state("A") will be called, 
       followed by the CallbackEntry() of the transitted-to state("B"), at this
       point it's considered we are `in the state of "B"`
       The next `spinOnce()` will repeat the above process for the new state
    * `spinToState(stateName)` will run the statechart until `stateName` state
       is reached, i.e. it's equivalent of a series calls to `spinOnce()` until
       we are `in the state of "stateName"`
* Async call
    * `spinAsync()` will spaw a new thread and run the state chart logic in that
       thread without blocking the calling thread.
       all the callbacks (statechange observer, state/transition action callbacks
       guard callbacks) will be called from the newly spawned thread thus locks
       must be provide by the callback functions if shared resources are contended
    * `stop()` will stop the asyncronously running state chart
##### Limitations on Async implementation
* This implementation does not support dynamic reconfiguration of the
  chart, i.e. you will need to `stop()` the chart to change some configurations
  like add/remove states, change state callbacks and transitions etc. if the 
  chart was (`spinAsync()`ed) previously.
  Future implementation might dynamic configuration if there's a need
* All the callbacks does not offer thread safty, there's two implication here:
    * If any of the callback functions will access shared resource with other
      thread, proper locks should be used
    * A `stop()` should be called before any of possible callback and their
      accessed varibles are going out of scope and/or being destructed.

### Exceptions
This library throws runtime_error exceptions at one of the following scenarios

* create a chart with an empty name `createChart("")`. This is not allowed and a
  chart should always created with a meaningful name
* create a state with an empty name `createState("")`. This is not allowed and a
  state should always created with a meaningful name
* create a transition `srcState->addTransition(dstState ...)` where the destine
  state `dstState` is not in the same chart as source state `srcState`.
  i.e. the `dstState` should either be created by the same chart `c` that
created `srcState` by `c->createState()`, or `dstState` is a subchart state
added to `srcState`'s containing chart `c` by `c->addSubchart()`.
