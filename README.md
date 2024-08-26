# Digital Atavism

Modules for [VCV Rack](https://vcvrack.com/) by [Shawn Laptiste](https://linktr.ee/lazerfalcon)

Add Digital Atavism through [the vcv library](https://library.vcvrack.com/?query=&brand=Digital+Atavism&tag=&license=).

----
## Table of Contents:

* [coin](#coin)

----

## coin

![coin](./images/coin.png)

#### Controls
  *  **FREQ** - The base frequency of the voice. 
  This CV input is 1V per octave.
  *  **FMOD** - The change in frequency.
  This CV input is 1V per octave.
  *  **TIME** - The delay before modifying the frequency.
  *  **HOLD** - The duration of the hold section of the HR envelope.
  *  **REL** - The duration of the release section of the HR envelope.
  *  **PUNCH** - The amount of drive applied during the hold section of the HR envelope.
  *  **TRIG** - The trigger CV input.
  With a wire connected to this input, the rising edge of a trigger or gate signal will activate the voice. When left unplugged, the module will act like an oscillator at the base frequency, and the **FMOD**, **TIME**, **HOLD**, **REL**, and **PUNCH** controls will have no effect.

#### Outputs

  *  **TRIG** - Outputs a trigger after the frequency modification delay time has elapsed.
  *  **ENV** - HR envelope output.
  *(Note that with positive values for **PUNCH**, this value will exceed +10V.)*
  *  **OUT** - Audio output.
