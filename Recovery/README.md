# Recovery System

How AstraNova brings the rocket back: a nose-cone separation event at apogee, commanded by a flight computer and deploying a parachute, with the recovery electronics kept fully isolated from the rest of the vehicle.

## Flight computer

Recovery is commanded by a **Mercury V1 altimeter and flight computer** ([product support](https://www.altimetercloud.com/support/mercuryv1/)), running on its own battery and electrically isolated from the payload electronics. It uses barometric pressure and onboard acceleration to determine flight phase and fire the ejection charge. It is UKRA qualified, externally disarmable before launch, and uses no tilt switches.

## Choosing the deployment method

We could not rely on black powder; access was uncertain under strict regulations and we were not able to source any. That left two charge options, both with prior competition heritage:

- a **nichrome burn wire** to melt through a plastic retention, and
- **flash paper** as a fast pressure charge.

Both have flown in competition, so each had a basis. Flash paper behaves most like black powder (a quick pressure pulse rather than a slow burn-through), so we selected flash paper, fired by an igniter.

A **mechanical deployment mechanism** (a servo or motor-driven release) was also considered and rejected; structures judged it too heavy for the mass budget.

## Flight computer versus the motor's timed ejection charge

Many teams deploy using the motor's built-in timed ejection charge, where a delay is set on the ground before flight. We chose a flight computer instead, deploying on **detected apogee** rather than a preset time, for a more controllable ascent and a deployment tied to the actual flight rather than an assumed one. That decision matters in the [trade-off](#flight-outcome-and-trade-off) below.

## Parachute and shock cords

A **24" Spherachute** with swivel slows the descent to within the recovery requirement; OpenRocket showed a descent rate around 9.5 m/s, comfortably under the 15 m/s limit. **Kevlar shock cords** over 2 m connect the nose cone and main airframe, sized to take the deployment load and prevent full separation, with eye bolts secured by nuts.

![Parachute deployment test](/Recovery/flashpaper_test.GIF)
*Figure 1: Flashpaper test for recovery section(parachute was tied together).*

## Flight outcome and trade-off

On the day, the parachute did not deploy. The rocket flew very straight on the way up, which speaks well of the aerodynamic design; it then carried that momentum over the top and pitched sideways at apogee rather than nosing straight down. The flight computer's near-freefall (under 1 g) deployment condition therefore never triggered, and the vehicle came in ballistic.

The honest trade-off worth recording:

- **The flight-computer choice was vindicated in one respect.** We were issued a different motor than planned on the day. A preset timed ejection charge, tuned to the original motor's expected apogee time, would almost certainly have fired at the wrong moment. Apogee-based deployment is immune to that, because it responds to the actual flight.
- **It also added complexity that most teams did not take on,** and that complexity is where the failure lived. Deployment was gated on a single accelerometer threshold, which a stable, non-vertical attitude at apogee never satisfies.

So the lesson is not "do not use a flight computer"; the flight computer was the more adaptable choice and protected us from exactly the motor swap that would have defeated a timed charge. The lesson is in the **deployment condition**: a barometric apogee detection (a positive-to-negative crossing of vertical velocity), or a multi-signal gate, would have fired regardless of attitude. Configure the trigger to the physics of apogee, not to a freefall assumption.

For the parallel power-side failure on the rideshare recorder, see the [avionics post-mortem](../Documentation/Avionics_RPD_and_Flight_Postmortem.md).
