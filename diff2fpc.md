This document attempts to list known differences to Free Pascal.

Keyword differences:
     Lacsap accepts `class` as synonym to `object`. 
     Lacsap accepts `otherwise:` instead of `else` in `case ... of`. (Now fixed)

Identifier parsing:
     Lacsap is case sensitive, FPC is not. (Now fixed)

Type differences:
     Lacsap uses `integer` as 32-bit value, `longint` as 64-bit
     integer. FPC has them as 16- and 32-bit values.
     `real` is a 64-bit double precisio float in Lacsap, 32-bit single
     precision float in FPC.
     


Builtin function differences:
     Lacsap has `clock`, `popcnt` and `panic` which are not in FPC.
     The `random` functions produce different results.
     The constant `pi` is has slightly different value.
