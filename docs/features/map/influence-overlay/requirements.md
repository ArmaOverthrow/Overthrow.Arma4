# Influence Overlay — Requirements

A new map overlay layer that when a map location is selected it draws dashed lines to any other locations that have currently active support modifiers, or locations that it is adding a modifier to

Examples:
- A town selected with a radio tower nearby controlled by the red faction. Red dashed line is drawn between them
- A radio tower  selected that is influencing the support of 3 nearby towns and controlled by the green faction. 3 green lines drawn to each town
and same for bases (no difference in the lines).
- A recently captured town is selected, lines drawn to nearby towns with the "RevolutionaryMomentum" modifier

If location data isnt currently stored and JIPd with the modifiers we can add it easily enough to build this layer

