Modding the OLED display
========================

The OLED display has a single VCC line that is connected to both
- the display controller chip, maximum voltage 3.6V
- the DC-DC step-up voltage regulator to produce +12V for the OLED panel,
  the DC-DC chip has a maximum input voltage of 5.5V

To optimize the battery life of the thermal camera, those two supply lines have
been split, so as to power the display controller with 3.3V, but connect the
DC-DC directly to the lithium battey voltage (3.5..4.2V).

NOTE: the display mod is mandatory, soldering an unmodded display will drive
the display controller with 4.2V and permanently damage it.

How to mod the OLED display
===========================

See display_mod.jpg

With a knife carefully cut the shown trace to separate the logic power trace
from the DC-DC power trace. Confirm with a multimeter that the trace is cut and
not accidentally shorted to ground (the trace immediately above and below is
ground).

With the same knife or similar tool carefully scrape the soldermask to the
left side of the trace, the one connecting to the display controller chip,
and solder a short piece of flexible copper wire.

Finally, when soldering the display to the thermal camera board, there is a
one-pin connector, J4 where the flexible wire going to the display controller
needs to be soldeered.
