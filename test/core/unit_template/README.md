# Unit testbench template

This is an example of a unit test bench that 
could be used as a starting point for new test benches.

The source here intends to meet the coding guidelines 
enforced with clang-format.  There is a script to help 
with this but it does not overwrite files automatically.

There some extra documentation and some standardization
in the port naming conventions.

The unit organization is as shown:

```
          i_dut_flush ---
                        |
    |--------|       |--------|       |--------|
    |        |---A->>|        |---C->>|        |
    | src    |       |  dut   |       |  sink  |
    |        |<<-B---|        |<<-D---|        |
    |--------|       |--------|       |--------|

 src.ports.o_instgrp_write  ----A->> dut.ports.i_instgrp_write
 src.ports.i_credits        <<--B--- dut.ports.o_restore_credits

 dut.ports.o_instgrp_write  ----C->> sink.ports.i_instgrp_write
 dut.ports.i_credits        <<--D--- sink.ports.o_restore_credits
```
