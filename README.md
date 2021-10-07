# JDFixer for Quest
Ever wanted to change your note offset with more scope and precision than the limited player settings options? Well, here you go. This mod is a port of https://github.com/zeph-yr/JDFixer with a couple changes.

On many maps, especially older ones and OSTs, the jump distance will be far away enough to negatively affect performance. Changing the note jump distance is generally regarded as changing the map itself and does not allow score submission, so this mod changes the offset instead.
## Features
- Quick settings access through side panel
- Set your jump distance anywhere from 1 to 50
- Remember last set jump distance
- Calculate and set jump distance for a configurable reaction time
- Automatically set values for each level and difficulty
## What do all these numbers mean?
The **jump distance** of a level is the absolute distance from the location the notes spawn to where you stand and slice them. It is calculated with an overcomplicated formula using the level's spawn offset, note jump speed, and BPM. **Note jump distance** is the speed that the notes move towards you, and **spawn offset** is an offset to the default jump distance.

The **reaction time** used by this mod is simply the time from the notes spawning until they reach you instead of the distance. A constant reaction time is much more universally applicable.
