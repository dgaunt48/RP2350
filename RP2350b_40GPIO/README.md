# RP2350b_40GPIO
Simple RP2350b dev board with 40 level shifted gpio pins and 3 bit VGA output.

## Source\SelfTest_40GPIO
Plug a VGA cable in and bridge the left 20 IO pins to the RIGHT 20.
Self test will then perform a simple bi-directional check on all 40 pins, plus check for shorts against the two adjacent pins.
Not exhaustive but will catch most GPIO errors on the board.

![Working Board](Images/Board.jpg?raw=true "Working Board")
