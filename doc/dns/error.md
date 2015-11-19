# NAME
Error -- DNS library specific errors

# LIBRARY
MeasurementKit (libmeasurement-kit, -lmeasurement-kit).

# SYNOPSIS
```C++
#include <measurement_kit/dns.hpp>

using namespace measurement_kit::dns;

FormatError;               // invalid response format
ServerFailedError;         // server failure
NotExistError;             // the name does not exist
NotImplementedError;       // query not implemented
RefusedError;              // the server refuses to reply
TruncatedError;            // response truncated
UknownError;               // internal evnds error
TimeoutError;              // query timed out
ShutdownError;             // evdns library was shut down
CancelError;               // user cancelled query
NoDataError;               // no data in the response

// Map evdns error code to a DNS error
common::Error dns_error(int code);
```

# DESCRIPTION

These are the errors triggered by MeasurementKit DNS library.

# HISTORY

DNS error classes appeared in MeasurementKit 0.1.