# Retrieve customized payload from TCP server #

This application will retrieve byte stream from TCP server, resolve specific payload and save to file.

Hard coded payload format is following:

sequence number(32bits) + chunk size(16bits) + chunk data + ...

# Environment #
Development envrionments
- Ubuntu 18.04
- g++ 7.4.0
- libcurl4-gnutls-dev 7.58.0

**Installation:**
```
> sudo apt-get update
> sudo apt-get install gcc-7 libcurl-gnutls-dev -y
```
The libcurl library default will be located in `/usr/lib/x86_64-linux-gnu/` and `/usr/include/x86_64-linux-gnu/`.

Make symlinks, add to environment variables for compiler, or copy required files for source code compiling.

# Usage #
**Compiling:**
```
> g++ simple_curl.cc -o simple_curl -lcurl
```

**Usage:**
```
> ./simple_curl <url> <file_path>
```

Given _**url**_ for TCP server address likes `http://foo.bar:1234` or `127.0.0.1:4321`.

Given _**file_path**_ to save output file likes `result.dat` or `/path/to/file.dat`.

# Others #
TCP socket relay site: https://ppng.io
```
> cat binfile | curl -T - https://ppng.io/whatyouwant
```