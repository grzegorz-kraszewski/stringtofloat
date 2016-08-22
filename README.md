# stringtofloat
Minimal dependency, easy to understand string to floating point number conversion.

## Requirements
* C compiler with unsigned 64-bit integer type and basic arithmetic on it (comparision, addition, subtraction, bit shifts).
* &lt;stdint.h&gt; header file or its equivalent defining `int32_t`, `uint32_t`, `uint64_t` types.
* It is assumed that `int` type is at least 16-bit wide.
* Size of other types and system endianess are not important.
* The C standard library is not required, unless a compiler needs it to provide the above features.
* No other libraries are required.

## Features
* Converts an ASCII string to IEEE 754 double precision number.
* Supports scientific notation, all common abbreviations (no zero before dot, ommitted plus signs).
* The code is easy to understand, algorithm is [explained in details](http://krashan.ppa.pl/articles/stringtofloat/).
* The code is splitted into parser and converter parts, which are independent. Parser is easy to modify to support non-ASCII or wide chars.
* Parser reads character by character, so can work with pipe style inputs. There is no limit for length of string, string is not buffered.
* Small in size. On most platforms the compiled code is less than 2 kB.
* Uses no floating point arithmetic by itself.
* Covers full range of double precision numbers with precise limit checking.
* Numbers out of range are converted to infinity with proper sign.
* Very small numbers are converted to zero with proper sign.

## Limitations
* No support for explicit "+Inf", "-Inf", "NaN" strings.
* Denormalized floats are never generated, zero of proper sign is returned instead.
* The rounding mode is always round towards zero.

## Tested platforms
* Linux on x86_64
* Linux on x86
* Linux on ARM
* Windows on x86_64
* MorphOS on PowerPC (32-bit)
* AmigaOS on M68k
