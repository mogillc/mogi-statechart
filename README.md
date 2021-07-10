# mogi_statechart

A UML state chart library

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
