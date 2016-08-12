## Trace malloc functions

ltrace and friends can be quite slow. This tool creates a shared
library that logs malloc+ functions to in-memory buffers and
periodically dumps the log in binary to a file.

The log can be unpacked with the associated utility.
