# Interesting Facts

## Should Autonomous run before Motor?

When multiple FreeRTOS tasks are ready at the same time (e.g. at t = 0 ms or t = 50 ms), the scheduler runs them in **priority order**. Currently: **Motor (4) → Autonomous (3) → Communication (2)**. So **Motor runs before Autonomous**.

---

### Why “Autonomous before Motor” can make sense

From a **data-flow** point of view:

- **Autonomous** produces commands (FORWARD, turn, etc.) and pushes them into `xCommandQueue`.
- **Motor** reads from `xCommandQueue` and drives the motors.

In a given 50 ms cycle, if **Autonomous runs first**, it can:

1. Compute the new decision for this cycle.
2. Enqueue it.
3. **Motor** then runs and consumes that **fresh** command.

So, in the same tick, you get: decide → then actuate. That can reduce decision-to-actuation latency.

---

### Why Motor is kept above Autonomous in the current design

The current choice (Motor 4, Autonomous 3) is mainly to **protect the actuation loop**:

1. **Motor’s 10 ms loop** must run very regularly for stable PWM and control. If Autonomous had **higher** priority than Motor, a long Autonomous cycle (scan, state machine, etc.) could **delay** Motor and add jitter to the 10 ms cycle.

2. **Safety**: Motor is the only task that actually drives the motors. Giving it higher priority than Autonomous makes it less likely that a long Autonomous run blocks Motor from applying a new command (including STOP) quickly.

3. **Queue in the middle**: The queue already decouples producer and consumer. The command from the *previous* Autonomous cycle (up to 50 ms old) is still valid; Motor can use it on its next 10 ms wake. For a 50 ms planner, that delay is usually acceptable.

So the idea is: **actuation (Motor) is more time-critical than planning (Autonomous)**, so Motor gets the higher priority.

---

### If you want Autonomous before Motor

To have Autonomous run **before** Motor when both are ready, **swap** their priorities, e.g.:

- **Autonomous:** 4  
- **Motor:** 3  

Order when all are ready: **Autonomous (4) → Motor (3) → Communication (2)**.

**Benefits:** In the ticks where both run (e.g. 0 ms, 50 ms), the command used by Motor is from **this** cycle, not the previous one.

**Risks:** If an Autonomous cycle sometimes overruns, Motor’s 10 ms cadence can be delayed and you may see more jitter on the motors.

---

### Practical recommendation

- **Keep the current setup (Motor 4, Autonomous 3)** if you want maximum regularity for the 10 ms motor loop and for safety.
- **Switch to Autonomous 4 and Motor 3** only if you need the smallest possible delay from “Autonomous decision” to “Motor acts” and you can guarantee that Autonomous stays well within its 50 ms budget (e.g. with profiling or a worst-case timing check).
