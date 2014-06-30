/* libzpaq.h - LIBZPAQ Version 6.51 header - Apr. 3, 2014.

  This software is provided as-is, with no warranty.
  I, Matt Mahoney, on behalf of Dell Inc., release this software into
  the public domain.   This applies worldwide.
  In some countries this may not be legally possible; if so:
  I grant anyone the right to use this software for any purpose,
  without any conditions, unless such conditions are required by law.

LIBZPAQ is a C++ library providing data compression and decompression
services using the ZPAQ level 2 format as described in
http://mattmahoney.net/zpaq/

An application wishing to use these services should #include "libzpaq.h"
and link to libzpaq.cpp. libzpaq recognizes the following options:

  -DDEBUG   Turn on assertion checks (slower).
  -DNOJIT   Don't assume x86-32 or x86-64 with SSE2 (slower).
  -Dunix    Without -DNOJIT, assume Unix (Linux, Mac) rather than Windows.

The application must provide an error handling function and derived
implementations of two abstract classes, Reader and Writer,
specifying the input and output byte streams. For example, to compress
from stdin to stdout (assuming binary I/O as in Linux):

  #include "libzpaq.h"
  #include <stdio.h>
  #include <stdlib.h>

  void libzpaq::error(const char* msg) {  // print message and exit
    fprintf(stderr, "Oops: %s\n", msg);
    exit(1);
  }

  class In: public libzpaq::Reader {
  public:
    int get() {return getchar();}  // returns byte 0..255 or -1 at EOF
  } in;

  class Out: public libzpaq::Writer {
  public:
    void put(int c) {putchar(c);}  // writes 1 byte 0..255
  } out;

  int main() {
    libzpaq::compress(&in, &out, 2);  // level may be 1, 2, or 3
  }

Compression has 3 built in levels, where 1 is fastest and 3 gives
the best compression. To decompress:

  libzpaq::decompress(&in, &out);

The function error() will be called with an English language message
in case of an unrecoverable error such as badly formatted compressed
input data or running out of memory. error() should not return.
In a multi-threaded application where ZPAQ blocks are being decompressed
in separate threads, error() should exit the thread, but other threads
may continue. Blocks are independent and libzpaq is thread safe.

Reader and Writer provide default implementations of read() and write()
for block I/O. You may override these with your own versions, which
might be faster. The default is to call get() or put() the appropriate
number of times. For example:

  // Read n bytes into buf[0..n-1] or to EOF, whichever is first.
  // Return the number of bytes actually read.
  int In::read(char* buf, int n) {return fread(buf, 1, n, stdin);}

  // Write buf[0..n-1]
  void Out::write(char* buf, int n) {fwrite(buf, 1, n, stdout);}

By default, compress() compresses to a single segment within a single
ZPAQ block with no header locator tag, filename, comment, or checksum.
decompress() will decompress any valid ZPAQ stream, which may contain
multiple blocks with multiple segments each. It will ignore filenames,
comments, and checksums. If a block is immediately preceded by a
(13 byte) locator tag, then the block can be extracted even if embedded
in arbitrary data (such as self-extractor code). decompress() will skip
over any non-ZPAQ data between tagged blocks without generating errors.

The Compressor and Decompresser classes allow you to write and read
these components individually. To compress:

  libzpaq::Compressor c;
  for (int i=0; i<num_blocks; ++i) {
    c.setOutput(&out);              // if omitted or NULL, discard output
    c.writeTag();                   // optional locator tag
    c.startBlock(2);                // compression level 1, 2, or 3
    for (int j=0; j<num_segments; ++j) {
      c.startSegment("filename", "comment");  // default NULL = empty
      c.setInput(&in);
      while (c.compress(1000));     // bytes to compress, default -1 = all
      c.endSegment(sha1.result());  // default NULL = don't save checksum
    }
    c.endBlock();
  }

Input and output can be set anywhere before the first input and output
operation, respectively. Input can be changed at any time. Output
should only be changed between blocks.

compress() will read the requested number of bytes or until in.get()
returns EOF (-1), whichever comes first, and return true if there is
more data to decompress. If the argument is omitted or -1, then it will
read to EOF and return false.

endSegment() writes a provided SHA-1 cryptographic hash checksum of the
input segment before any preprocessing. It is safe to assume that two
inputs with the same hash are identical. To compute a hash you can use
the SHA1 class:

  libzpaq::SHA1 sha1;
  int ch;
  while ((ch=getchar())!=EOF)
    sha1.put(ch);
  printf("Size is %1.0f or %1.0f bytes\n", sha1.size(), double(sha1.usize()));

size() returns the number of bytes read as a double, and usize() as a
64 bit integer. result() returns a pointer to the 20 byte hash and
resets the size to 0. The hash (not just the pointer) should be copied
before the next call to result() if you want to save it.

To decompress individual blocks and segments and retrieve the filenames,
comments, data, and hashes of each segment (in exactly this order):

  libzpaq::Decompresser d;
  double memory;                         // bytes required to decompress
  Out filename, comment;
  char sha1out[21];
  d.setInput(&in);
  while (d.findBlock(&memory)) {         // default is NULL
    while (d.findFilename(&filename)) {  // default is NULL
      d.readComment(&comment);           // default is NULL
      d.setOutput(&out);                 // if omitted or NULL, discard output
      d.setSHA1(&sha1);                  // optional
      while (d.decompress(1000));        // bytes to decode, default is all
      d.readSegmentEnd(sha1out);         // {0} or {1,hash[20]}
      if (sha1out[0]==1 && memcmp(sha1.result(), sha1out+1, 20))
        error("checksum error");
    }
  }

findBlock() scans the input for the next ZPAQ block and returns true
if found. It optionally sets memory to the approximate number of bytes
that it will allocate at the first call to decompress().

findFilename() finds the next segment and returns false if there are
no more in the current block. It optionally writes the saved filename.

readComment() optionally writes the comment. It must be called
after reading the filename and before decompressing.

setSHA1() specifies an SHA1 object for computing a hash of the segment.
It may be omitted if you do not want to compute a hash.

decompress() decodes the requested number of bytes, postprocesses them,
and writes them to out. For the 3 built in compression levels, this
is the same as the number of bytes output, but it may be different if
postprocessing was used. It returns true until there is no more data
to decompress in the current segment. The default (-1) is to decompress the
whole segment.

readSegmentEnd() skips any remaining data not yet decompressed in the
segment and writes 21 bytes, either a 0 if no hash was saved, 
or a 1 followed by the 20 byte saved hash. If any data is skipped,
then all data in the remaining segments in the current block must
also be skipped.

ZPAQ supports arbitrary compression algorithms in addition to the
3 built in levels. For example, level 1 compression could alternatively
be specified using the ZPAQL language description of the compression
algorithm:

  int args[9]={0}
  c.startBlock(
    "(min.cfg - equivalent to level 1) "
    "comp 1 2 0 0 2 (log array sizes hh,hm,ph,pm and number of components n) "
    "  0 icm 16    (order 2 indirect context model using 4 MB memory) "
    "  1 isse 19 0 (order 4 indirect secondary symbol estimator, 32 MB) "
    "hcomp (context computation, input is last modeled byte in A) "
    "  *b=a a=0 (save in rotating buffer M pointed to by B) "
    "  d=0 hash b-- hash *d=a (put order 2 context hash in H[0] pointed by D)"
    "  d++ b-- hash b-- hash *d=a (put order 4 context in H[1]) "
    "  halt "
    "end " (no pre/post processing) ",
    args,     // Arguments $1 through $9 to ZPAQL code (unused, can be NULL)
    &out);    // Writer* to write pcomp command (default is NULL)

The first argument is a description of the compression algorithm in
the ZPAQL language. It is compiled into byte code and saved in the
archive block header so that the decompressor knows how read the data.
A ZPAQL program accepts up to 9 numeric arguments, which should be
passed in array.

A decompression algorithm has two optional parts, a context mixing
model and a postprocessor. The context model is identical for both
the compressor and decompressor, so is used in both instances. The
postprocessor, if present, is generally different, which presents
the possibility that the user supplied code may not restore the
original data exactly. It is assumed that the input has already been
preprocessed by the application but that the hash supplied to endSegment()
is of the original input before preprocessing. The fullowing functions
allow you to test the postprocesser during compression:

  c.setVerify(true); // before c.compress(), may run slower
  c.getSize();       // return 64 bit size of postprocessed output
  c.getChecksum();   // after c.readSegmentEnd(), return hash, reset size to 0

This example has no postprocessor, but if it did, then setVerify(true)
would cause compress() to run the preprocessed input through the
postprocessor and into a SHA1 in parallel with compression. Then,
getChecksum() would return the hash which could be compared with
the hash of the input computed by the application. Also,

  int64_t size;
  c.endSegmentChecksum(&size);

instead of c.endSegment() will automatically add the computed checksum
if setVerify is true and return the checksum, whether or not there
is a postprocessor. If &size is not NULL then the segment size is written
to size. If setVerify is false, then no checksum is saved
and the function returns 0 with size not written.

A context model consists of two parts, an array COMP of n components,
and some code HCOMP that computes contexts for the components.
The model compresses one bit at a time (MSB to LSB order) by computing
a probability that the next bit will be a 1, and then arithmetic
coding that bit. Better predictions compress smaller.

If n is 0 then the data is uncompressed. Otherwise, there is an array
of n = 1..255 components each taking a context and possibly the
predictions of previous components as input and outputting a new
prediction. The output of the last prediction is used to encode the
bit. After encoding, the state of each component is updated to
reduce the prediction error when the same context occurs again.
Components are as follows. Most arguments have range 0...255.

  CONST c          predict a 1 (c > 128) or 0 (c < 128).
  CM s t           context model with 2^s contexts, learning rate 1/4t.
  ICM s            indirect context model with 2^(s+6) contexts.
  MATCH s b        match model with 2^s context hashes and 2^b history.
  AVG j k wt       average components j and k with weight wt/256 for j.
  MIX2 s j k r x   average j and k with 2^s contexts, rate r, mask x.
  MIX  s j m r x   average j..j+m-1 with 2^s contexts, rate r, mask x.
  ISSE s j         adjust prediction j using 2^(s+6) indirect contexts.
  SSE s j t1 t2    adjust j using 2^s direct contexts, rate 1/t1..1/4t2.

A CONST predicts a 1 with probability 1/(1+exp((128-c)/16)), i.e
numbers near 0 or 255 are the most confident.
  
A CM maps a context to a prediction and a count. It is updated by
adjusting the prediction to reduce the error by 1/count and incrementing
the count up to 4t.

A ICM maps a s+10 bit context hash to a bit history (8 bit state)
representing a bounded count of zeros and ones previously seen in the
context and which bit was last. The bit history is mapped to a
prediction, which is updated by reducing the error by 1/1024.
The initial prediction is estimated from the counts represented by each
bit history.

A MATCH looks up a context hash and predicts whatever bit came next
following the previous occurrence in the history buffer. The strength
of the prediction depends on the match length.

AVG, MIX2, and MIX perform weighted averaging of predictions in the
logistic domain (log(p/(1-p))). AVG uses a fixed weight. MIX2 and MIX
adjust the weights (selected by context) to reduce prediction error
by a rate that increases with r. The mask is AND-ed with the current
partially coded byte to compute that context. Normally it is 255.
A MIX takes a contiguous range of m components as input.

ISSE adjusts a prediction using a bit history (as with an ICM) to
select a pair of weights for a 2 input MIX. It mixes the input
prediction with a constant 1 in the logistic domain.

SSE adjusts a logistic prediction by quantizing it to 32 levels and
selecting a new prediction from a table indexed by context, interpolating
between the nearest two steps. The nearest prediction error is
reduced by 1/count where count increments from t1 to 4*t2.

Contexts are computed and stored in an array H of 32 bit unsigned
integers by the HCOMP program written in ZPAQL. The program is called
after encoding a whole byte. To form a complete context, these values
are combined with the previous 0 to 7 bits of the current parital byte.
The method depends on the component type as follows:

  CM: H[i]    XOR hmap4(c).
  ICM, ISSE:  hash table lookup of (H[i]*16+c) on nibble boundaries.
  MIX2, MIX:  H[i] + (c AND x).
  SSE:        H[i] + c.

where c is the previous bits with a leading 1 bit (1, 1x, 1xx, ...,
1xxxxxxx where x is a previously coded bit). hmap4(c) maps c
to a 9 bit value to reduce cache misses. The first nibble is
mapped as before and the second nibble with 1xxxx in the high
5 bits. For example, after 6 bits, where c = 1xxxxxx,
hmap4(c) = 1xxxx01xx with the bits in the same order.

There are two ZPAQL virtual machines, HCOMP to compute contexts
and PCOMP to post-process the decoded output. Each has the
following state:

  PC: 16 bit program counter.
  A, B, C, D, R0...R255: 32 bit unsigned registers.
  F: 1 bit condition register.
  H: array of 2^h 32 bit unsigned values (output for HCOMP).
  M: array of 2^m 8 bit unsigned values.

All values are initialized to 0 at the beginning of a block
and retain their values between calls. There are two machines.
HCOMP is called after coding each byte with the value of that
byte in A. PCOMP, if present, is called once for each decoded
byte with that byte in A, and once more at the end of each
segment with 2^32 - 1 in A.

Normally, A is an accumulator. It is the destination of all
binary operations except assignment. The low m bits of B and
C index M. The low h bits of D indexes H. We write *B, *C, *D
to refer to the elements they point to. The instruction set
is as follows, where X is A, B, C, D, *B, *C, *D except as
indicated. X may also be a constant 0...255, written with
a leading space if it appears on the right side of an operator,
e.g. "*B= 255". Instructions taking a numeric argument are 2 bytes,
otherwise 1. Arithmetic is modulo 2^32.

  X<>A    Swap X with A (X cannot be A).
  X++     Add 1.
  X--     Subtract 1.
  X!      Complement bits of X.
  X=0     Clear X (1 byte instruction).
  X=X     Assignment to left hand side.
  A+=X    Add to A
  A-=X    Subtract from A
  A*=X    Multipy
  A/=X    Divide. If X is 0 then A=0.
  A%=X    Mod. If X is 0 then A=0.
  A&=X    Clear bits of A that are 0 in X.
  A&~X    Clear bits of A that are 1 in X.
  A|=X    Set bits of A that are 1 in X.
  A^=X    Complement bits of A that are set in X.
  A<<=X   Shift A left by (X mod 32) bits.
  A>>=X   Shift right (zero fill) A by (X mod 32) bits.
  A==X    Set F=1 if equal else F=0.
  A<X     Set F=1 if less else F=0.
  A>X     Set F=1 if greater else F=0.
  X=R N   Set A,B,C,D to RN (R0...R255).
  R=A N   Set R0...R255 to A.
  JMP N   Jump N=-128...127 bytes from next instruction.
  JT N    Jump N=-128...127 if F is 1.
  JF N    Jump N=-128...127 if F is 0.
  LJ N    Long jump to location 0...65535 (only 3 byte instruction).
  OUT     Output A (PCOMP only).
  HASH    A=(A+*B+512)*773.
  HASHD   *D=(*D+A+512)*773.
  HALT    Return at end of program.
  ERROR   Fail if executed.

Rather than using jump instructions, the following constructs are
allowed and translated appropriately.

  IF ... ENDIF              Execute if F is 1.
  IFNOT ... ENDIF           Execute if F is 0.
  IF ... ELSE ... ENDIF     Execute first part if F is 1 else second part.
  IFNOT ... ELSE ... ENDIF  Execute first part if F is 0 else second part.
  DO ... WHILE              Loop while F is 1.
  DO ... UNTIL              Loop while F is 0.
  DO ... FOREVER            Loop unconditionally.

Forward jumps (IF, IFNOT, ELSE) will not compile if beyond 127
instructions. In that case, use the long form (IFL, IFNOTL, ELSEL).
DO loops automatically use long jumps if needed. IF and DO loops
may intersect. For example, DO ... IF ... FOREVER ENDIF is equivalent
to a while-loop.

A config argument without a postprocessor has the following syntax:

  COMP hh hm ph pm n
    i COMP args...
  HCOMP
    zpaql...
  END (or POST 0 END for backward compatibility)

With a postprocessor:

  COMP hh hm ph pm n
    i COMP args...
  HCOMP
    zpaql...
  PCOMP command args... ;
    zpaql...
  END

In HCOMP, H and M have sizes 2^hh and 2^hm respectively. In PCOMP,
H and M have sizes 2^ph and 2^pm respectively. There are n components,
which must be numbered i = 0 to n-1. If a postprocessor is used, then
"command args..." is written to the Writer* passed as the 4'th argument,
but otherwise ignored. A typical use in a development environment might
be to call an external program that will be passed two additional
arguments on the command line, the input and output file names
respectively.

You can pass up to 9 signed numeric arguments in args[]. In any
place that a number "N" is allowed, you can write "$M" or "$M+N"
(like "$1" or $9+25") and value args[M-1]+N will be substituted.

ZPAQL allows (nested) comments in parenthesis. It is not case sensitive.
If there are input errors, then error() will report the error. If the
string contains newlines, it will report the line number of the error.

ZPAQL is compiled internally into a byte code, and then to native x86
32 or 64 bit code (unless compiled with -DNOJIT, in which case the
byte code is interpreted). You can also specify the algorithm directly
in byte code, although this is less convenient because it requires two
steps:

  c.startBlock(hcomp);      // COMP and HCOMP at start of block
  c.postProcess(pcomp, 0);  // PCOMP right before compress() in first segment

This is necessary because the COMP and HCOMP sections are stored in
the block header, but the PCOMP section is compressed in the first
segment after the filename and comment but before any data.

To retrive compiled byte code in suitable format after startBlock():

  c.hcomp(&out);      // writes COMP and HCOMP sections
  c.pcomp(&out);      // writes PCOMP section if any

Or during decompression:

  d.hcomp(&out);      // valid after findBlock()
  d.pcomp(&out);      // valid after decompress(0) in first segment

Both versions of pcomp() write nothing and return false if there is no
PCOMP section. The output of hcomp() and pcomp() may be passed to the
input of startBlock() and postProcess(). These are strings in which the
first 2 bytes encode the length of the rest of the string, least
significant byte first. Alternatively, postProcess() allows the length to
be omitted and passed separately as the second argument. In the case
of decompression, the HCOMP and PCOMP strings are read from the archive.
The preprocessor command (from "PCOMP cmd ;") is not saved in the compressed
data.

OTHER CLASSES

The Array template class is convenient for creating arrays aligned on
64 byte addresses. It calls error("Out of memory") if needed.
It is used as follows:

  libzpaq::Array<T> a(n);  // array a[0]..a[n-1] of type T, zeroed
  a.resize(n);             // change size and zero contents
  a[i]                     // i'th element
  a(i)                     // a[i%n], valid only if n is a power of 2
  a.size()                 // n (as a size_t)
  a.isize()                // n (as a signed int)

T should be a simple type without constructors or destructors. Arrays
cannot be copied or assigned. You can also specify the size:

  Array<T> a(n, e);  // n << e
  a.resize(n, e);    // n << e

which is equivalent to n << e except that it calls error("Array too big")
rather than overflow if n << e would require more than 32 bits. If
compiled with -DDEBUG, then bounds are checked at run time.

There is a class libzpaq::SHA256 with an identical interface to SHA1
which returns a 32 byte SHA-256 hash. It is not part of the ZPAQ standard.

The AES_CTR class allows encryption in CTR mode with 128, 192, or 256
bit keys. The public members are:

class AES_CTR {
public:
  AES_CTR(const char* key, int keylen, char* iv=0);
  void encrypt(U32 s0, U32 s1, U32 s2, U32 s3, unsigned char* ct);
  void encrypt(char* buf, int n, U64 offset);
};

The constructor initializes with a 16, 24, or 32 byte key. The length
is given by keylen. iv can be an 8 byte string or NULL. If not NULL
then iv0, iv1 are initialized with iv[0..7] in big-endian order, else 0.

encrypt(s0, s1, s2, s3, ct) encrypts a plaintext block divided into
4 32-bit words MSB first. The first byte of plaintext is the high 8
bits of s0. The output is to ct[16].

encrypt(buf, n, offset) encrypts or decrypts an n byte slice of a string
starting at offset. The i'th 16 byte block is encrypted by XOR with
the result (in ct) of encrypt(iv0, iv1, i>>32, i&0xffffffff, ct) starting
with i = 0. For example:

  AES_CTR a("a 128 bit key!!!", 16);
  char buf[500];             // some data 
  a.encrypt(buf, 100, 0);    // encrypt first 100 bytes
  a.encrypt(buf, 400, 100);  // encrypt next 400 bytes
  a.encrypt(buf, 500, 0);    // decrypt in one step

stretchKey(char* out, const char* in, const char* salt);

Generate a 32 byte key out[0..31] from key[0..31] and salt[0..31]
using scrypt(key, salt, N=16384, r=8, p=1). key[0..31] should be
the SHA-256 hash of the password. With these parameters, the function
uses 0.1 to 0.3 seconds and 16 MiB memory.
Scrypt is defined in http://www.tarsnap.com/scrypt/scrypt.pdf

Other classes and functions defined here are for internal use.
Use at your own risk.
*/

#ifndef LIBZPAQ_H
#define LIBZPAQ_H

#ifndef DEBUG
#define NDEBUG 1
#endif
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

namespace libzpaq {

// 1, 2, 4, 8 byte unsigned integers
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

// Tables for parsing ZPAQL source code
extern const char* compname[256];    // list of ZPAQL component types
extern const int compsize[256];      // number of bytes to encode a component
extern const char* opcodelist[272];  // list of ZPAQL instructions

// Callback for error handling
extern void error(const char* msg);

// Virtual base classes for input and output
// get() and put() must be overridden to read or write 1 byte.
// read() and write() may be overridden to read or write n bytes more
// efficiently than calling get() or put() n times.
class Reader {
public:
  virtual int get() = 0;  // should return 0..255, or -1 at EOF
  virtual int read(char* buf, int n); // read to buf[n], return no. read
  virtual ~Reader() {}
};

class Writer {
public:
  virtual void put(int c) = 0;  // should output low 8 bits of c
  virtual void write(const char* buf, int n);  // write buf[n]
  virtual ~Writer() {}
};

// Read 16 bit little-endian number
int toU16(const char* p);

// An Array of T is cleared and aligned on a 64 byte address
//   with no constructors called. No copy or assignment.
// Array<T> a(n, ex=0);  - creates n<<ex elements of type T
// a[i] - index
// a(i) - index mod n, n must be a power of 2
// a.size() - gets n
template <typename T>
class Array {
  T *data;     // user location of [0] on a 64 byte boundary
  size_t n;    // user size
  int offset;  // distance back in bytes to start of actual allocation
  void operator=(const Array&);  // no assignment
  Array(const Array&);  // no copy
public:
  Array(size_t sz=0, int ex=0): data(0), n(0), offset(0) {
    resize(sz, ex);} // [0..sz-1] = 0
  void resize(size_t sz, int ex=0); // change size, erase content to zeros
  ~Array() {resize(0);}  // free memory
  size_t size() const {return n;}  // get size
  int isize() const {return int(n);}  // get size as an int
  T& operator[](size_t i) {assert(n>0 && i<n); return data[i];}
  T& operator()(size_t i) {assert(n>0 && (n&(n-1))==0); return data[i&(n-1)];}
};

// Change size to sz<<ex elements of 0
template<typename T>
void Array<T>::resize(size_t sz, int ex) {
  assert(size_t(-1)>0);  // unsigned type?
  while (ex>0) {
    if (sz>sz*2) error("Array too big");
    sz*=2, --ex;
  }
  if (n>0) {
    assert(offset>0 && offset<=64);
    assert((char*)data-offset);
    ::free((char*)data-offset);
  }
  n=0;
  if (sz==0) return;
  n=sz;
  const size_t nb=128+n*sizeof(T);  // test for overflow
  if (nb<=128 || (nb-128)/sizeof(T)!=n) error("Array too big");
  data=(T*)::calloc(nb, 1);
  if (!data) n=0, error("Out of memory");
  offset=64-(((char*)data-(char*)0)&63);
  assert(offset>0 && offset<=64);
  data=(T*)((char*)data+offset);
}

//////////////////////////// SHA1 ////////////////////////////

// For computing SHA-1 checksums
class SHA1 {
public:
  void put(int c) {  // hash 1 byte
    U32& r=w[len0>>5&15];
    r=(r<<8)|(c&255);
    if (!(len0+=8)) ++len1;
    if ((len0&511)==0) process();
  }
  double size() const {return len0/8+len1*536870912.0;} // size in bytes
  uint64_t usize() const {return len0/8+(U64(len1)<<29);} // size in bytes
  const char* result();  // get hash and reset
  SHA1() {init();}
private:
  void init();      // reset, but don't clear hbuf
  U32 len0, len1;   // length in bits (low, high)
  U32 h[5];         // hash state
  U32 w[16];        // input buffer
  char hbuf[20];    // result
  void process();   // hash 1 block
};

//////////////////////////// SHA256 //////////////////////////

// For computing SHA-256 checksums
// http://en.wikipedia.org/wiki/SHA-2
class SHA256 {
public:
  void put(int c) {  // hash 1 byte
    unsigned& r=w[len0>>5&15];
    r=(r<<8)|(c&255);
    if (!(len0+=8)) ++len1;
    if ((len0&511)==0) process();
  }
  double size() const {return len0/8+len1*536870912.0;} // size in bytes
  uint64_t usize() const {return len0/8+(U64(len1)<<29);} //size in bytes
  const char* result();  // get hash and reset
  SHA256() {init();}
private:
  void init();           // reset, but don't clear hbuf
  unsigned len0, len1;   // length in bits (low, high)
  unsigned s[8];         // hash state
  unsigned w[16];        // input buffer
  char hbuf[32];         // result
  void process();        // hash 1 block
};

//////////////////////////// AES /////////////////////////////

// For encrypting with AES in CTR mode.
// The i'th 16 byte block is encrypted by XOR with AES(i)
// (i is big endian or MSB first, starting with 0).
class AES_CTR {
  U32 Te0[256], Te1[256], Te2[256], Te3[256], Te4[256]; // encryption tables
  U32 ek[60];  // round key
  int Nr;  // number of rounds (10, 12, 14 for AES 128, 192, 256)
  U32 iv0, iv1;  // first 8 bytes in CTR mode
public:
  AES_CTR(const char* key, int keylen, const char* iv=0);
    // Schedule: keylen is 16, 24, or 32, iv is 8 bytes or NULL
  void encrypt(U32 s0, U32 s1, U32 s2, U32 s3, unsigned char* ct);
  void encrypt(char* buf, int n, U64 offset);  // encrypt n bytes of buf
};

//////////////////////////// stretchKey //////////////////////

// Strengthen password pw[0..pwlen-1] and salt[0..saltlen-1]
// to produce key buf[0..buflen-1]. Uses O(n*r*p) time and 128*r*n bytes
// of memory. n must be a power of 2 and r <= 8.
void scrypt(const char* pw, int pwlen,
            const char* salt, int saltlen,
            int n, int r, int p, char* buf, int buflen);

// Generate a strong key out[0..31] key[0..31] and salt[0..31].
// Calls scrypt(key, 32, salt, 32, 16384, 8, 1, out, 32);
void stretchKey(char* out, const char* key, const char* salt);

//////////////////////////// ZPAQL ///////////////////////////

// Symbolic constants, instruction size, and names
typedef enum {NONE,CONS,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE} CompType;
extern const int compsize[256];

// A ZPAQL machine COMP+HCOMP or PCOMP.
class ZPAQL {
public:
  ZPAQL();
  ~ZPAQL();
  void clear();           // Free memory, erase program, reset machine state
  void inith();           // Initialize as HCOMP to run
  void initp();           // Initialize as PCOMP to run
  double memory();        // Return memory requirement in bytes
  void run(U32 input);    // Execute with input
  int read(Reader* in2);  // Read header
  bool write(Writer* out2, bool pp); // If pp write PCOMP else HCOMP header
  int step(U32 input, int mode);  // Trace execution (defined externally)

  Writer* output;         // Destination for OUT instruction, or 0 to suppress
  SHA1* sha1;             // Points to checksum computer
  U32 H(int i) {return h(i);}  // get element of h

  void flush();           // write outbuf[0..bufptr-1] to output and sha1
  void outc(int c) {      // output byte c (0..255) or -1 at EOS
    if (c<0 || (outbuf[bufptr]=c, ++bufptr==outbuf.isize())) flush();
  }

  // ZPAQ1 block header
  Array<U8> header;   // hsize[2] hh hm ph pm n COMP (guard) HCOMP (guard)
  int cend;           // COMP in header[7...cend-1]
  int hbegin, hend;   // HCOMP/PCOMP in header[hbegin...hend-1]

private:
  // Machine state for executing HCOMP
  Array<U8> m;        // memory array M for HCOMP
  Array<U32> h;       // hash array H for HCOMP
  Array<U32> r;       // 256 element register array
  Array<char> outbuf; // output buffer
  int bufptr;         // number of bytes in outbuf
  U32 a, b, c, d;     // machine registers
  int f;              // condition flag
  int pc;             // program counter
  int rcode_size;     // length of rcode
  U8* rcode;          // JIT code for run()

  // Support code
  int assemble();  // put JIT code in rcode
  void init(int hbits, int mbits);  // initialize H and M sizes
  int execute();  // interpret 1 instruction, return 0 after HALT, else 1
  void run0(U32 input);  // default run() if not JIT
  void div(U32 x) {if (x) a/=x; else a=0;}
  void mod(U32 x) {if (x) a%=x; else a=0;}
  void swap(U32& x) {a^=x; x^=a; a^=x;}
  void swap(U8& x)  {a^=x; x^=a; a^=x;}
  void err();  // exit with run time error
};

///////////////////////// Component //////////////////////////

// A Component is a context model, indirect context model, match model,
// fixed weight mixer, adaptive 2 input mixer without or with current
// partial byte as context, adaptive m input mixer (without or with),
// or SSE (without or with).

struct Component {
  size_t limit;   // max count for cm
  size_t cxt;     // saved context
  size_t a, b, c; // multi-purpose variables
  Array<U32> cm;  // cm[cxt] -> p in bits 31..10, n in 9..0; MATCH index
  Array<U8> ht;   // ICM/ISSE hash table[0..size1][0..15] and MATCH buf
  Array<U16> a16; // MIX weights
  void init();    // initialize to all 0
  Component() {init();}
};

////////////////////////// StateTable ////////////////////////

// Next state table
class StateTable {
public:
  U8 ns[1024]; // state*4 -> next state if 0, if 1, n0, n1
  int next(int state, int y) {  // next state for bit y
    assert(state>=0 && state<256);
    assert(y>=0 && y<4);
    return ns[state*4+y];
  }
  int cminit(int state) {  // initial probability of 1 * 2^23
    assert(state>=0 && state<256);
    return ((ns[state*4+3]*2+1)<<22)/(ns[state*4+2]+ns[state*4+3]+1);
  }
  StateTable();
};

///////////////////////// Predictor //////////////////////////

// A predictor guesses the next bit
class Predictor {
public:
  Predictor(ZPAQL&);
  ~Predictor();
  void init();          // build model
  int predict();        // probability that next bit is a 1 (0..4095)
  void update(int y);   // train on bit y (0..1)
  int stat(int);        // Defined externally
  bool isModeled() {    // n>0 components?
    assert(z.header.isize()>6);
    return z.header[6]!=0;
  }
private:

  // Predictor state
  int c8;               // last 0...7 bits.
  int hmap4;            // c8 split into nibbles
  int p[256];           // predictions
  U32 h[256];           // unrolled copy of z.h
  ZPAQL& z;             // VM to compute context hashes, includes H, n
  Component comp[256];  // the model, includes P

  // Modeling support functions
  int predict0();       // default
  void update0(int y);  // default
  int dt2k[256];        // division table for match: dt2k[i] = 2^12/i
  int dt[1024];         // division table for cm: dt[i] = 2^16/(i+1.5)
  U16 squasht[4096];    // squash() lookup table
  short stretcht[32768];// stretch() lookup table
  StateTable st;        // next, cminit functions
  U8* pcode;            // JIT code for predict() and update()
  int pcode_size;       // length of pcode

  // reduce prediction error in cr.cm
  void train(Component& cr, int y) {
    assert(y==0 || y==1);
    U32& pn=cr.cm(cr.cxt);
    U32 count=pn&0x3ff;
    int error=y*32767-(cr.cm(cr.cxt)>>17);
    pn+=(error*dt[count]&-1024)+(count<cr.limit);
  }

  // x -> floor(32768/(1+exp(-x/64)))
  int squash(int x) {
    assert(x>=-2048 && x<=2047);
    return squasht[x+2048];
  }

  // x -> round(64*log((x+0.5)/(32767.5-x))), approx inverse of squash
  int stretch(int x) {
    assert(x>=0 && x<=32767);
    return stretcht[x];
  }

  // bound x to a 12 bit signed int
  int clamp2k(int x) {
    if (x<-2048) return -2048;
    else if (x>2047) return 2047;
    else return x;
  }

  // bound x to a 20 bit signed int
  int clamp512k(int x) {
    if (x<-(1<<19)) return -(1<<19);
    else if (x>=(1<<19)) return (1<<19)-1;
    else return x;
  }

  // Get cxt in ht, creating a new row if needed
  size_t find(Array<U8>& ht, int sizebits, U32 cxt);

  // Put JIT code in pcode
  int assemble_p();
};

//////////////////////////// Decoder /////////////////////////

// Decoder decompresses using an arithmetic code
class Decoder {
public:
  Reader* in;        // destination
  Decoder(ZPAQL& z);
  int decompress();  // return a byte or EOF
  int skip();        // skip to the end of the segment, return next byte
  void init();       // initialize at start of block
  int stat(int x) {return pr.stat(x);}
private:
  U32 low, high;     // range
  U32 curr;          // last 4 bytes of archive
  Predictor pr;      // to get p
  enum {BUFSIZE=1<<16};
  Array<char> buf;   // input buffer of size BUFSIZE bytes
    // of unmodeled data. buf[low..high-1] is input with curr
    // remaining in sub-block.
  int decode(int p); // return decoded bit (0..1) with prob. p (0..65535)
  void loadbuf();    // read unmodeled data into buf to EOS
};

/////////////////////////// PostProcessor ////////////////////

class PostProcessor {
  int state;   // input parse state: 0=INIT, 1=PASS, 2..4=loading, 5=POST
  int hsize;   // header size
  int ph, pm;  // sizes of H and M in z
public:
  ZPAQL z;     // holds PCOMP
  PostProcessor(): state(0), hsize(0), ph(0), pm(0) {}
  void init(int h, int m);  // ph, pm sizes of H and M
  int write(int c);  // Input a byte, return state
  int getState() const {return state;}
  void setOutput(Writer* out) {z.output=out;}
  void setSHA1(SHA1* sha1ptr) {z.sha1=sha1ptr;}
};

//////////////////////// Decompresser ////////////////////////

// For decompression and listing archive contents
class Decompresser {
public:
  Decompresser(): z(), dec(z), pp(), state(BLOCK), decode_state(FIRSTSEG) {}
  void setInput(Reader* in) {dec.in=in;}
  bool findBlock(double* memptr = 0);
  void hcomp(Writer* out2) {z.write(out2, false);}
  bool findFilename(Writer* = 0);
  void readComment(Writer* = 0);
  void setOutput(Writer* out) {pp.setOutput(out);}
  void setSHA1(SHA1* sha1ptr) {pp.setSHA1(sha1ptr);}
  bool decompress(int n = -1);  // n bytes, -1=all, return true until done
  bool pcomp(Writer* out2) {return pp.z.write(out2, true);}
  void readSegmentEnd(char* sha1string = 0);
  int stat(int x) {return dec.stat(x);}
private:
  ZPAQL z;
  Decoder dec;
  PostProcessor pp;
  enum {BLOCK, FILENAME, COMMENT, DATA, SEGEND} state;  // expected next
  enum {FIRSTSEG, SEG, SKIP} decode_state;  // which segment in block?
};

/////////////////////////// decompress() /////////////////////

void decompress(Reader* in, Writer* out);

//////////////////////////// Encoder /////////////////////////

// Encoder compresses using an arithmetic code
class Encoder {
public:
  Encoder(ZPAQL& z, int size=0):
    out(0), low(1), high(0xFFFFFFFF), pr(z) {}
  void init();
  void compress(int c);  // c is 0..255 or EOF
  int stat(int x) {return pr.stat(x);}
  Writer* out;  // destination
private:
  U32 low, high; // range
  Predictor pr;  // to get p
  Array<char> buf; // unmodeled input
  void encode(int y, int p); // encode bit y (0..1) with prob. p (0..65535)
};

//////////////////////////// Compiler ////////////////////////

// Input ZPAQL source code with args and store the compiled code
// in hz and pz and write pcomp_cmd to out2.

class Compiler {
public:
  Compiler(const char* in, int* args, ZPAQL& hz, ZPAQL& pz, Writer* out2);
private:
  const char* in;  // ZPAQL source code
  int* args;       // Array of up to 9 args, default NULL = all 0
  ZPAQL& hz;       // Output of COMP and HCOMP sections
  ZPAQL& pz;       // Output of PCOMP section
  Writer* out2;    // Output ... of "PCOMP ... ;"
  int line;        // Input line number for reporting errors
  int state;       // parse state: 0=space -1=word >0 (nest level)

  // Symbolic constants
  typedef enum {NONE,CONS,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE,
    JT=39,JF=47,JMP=63,LJ=255,
    POST=256,PCOMP,END,IF,IFNOT,ELSE,ENDIF,DO,
    WHILE,UNTIL,FOREVER,IFL,IFNOTL,ELSEL,SEMICOLON} CompType;

  void syntaxError(const char* msg, const char* expected=0); // error()
  void next();                     // advance in to next token
  bool matchToken(const char* tok);// in==token?
  int rtoken(int low, int high);   // return token which must be in range
  int rtoken(const char* list[]);  // return token by position in list
  void rtoken(const char* s);      // return token which must be s
  int compile_comp(ZPAQL& z);      // compile either HCOMP or PCOMP

  // Stack of n elements
  class Stack {
    libzpaq::Array<U16> s;
    size_t top;
  public:
    Stack(int n): s(n), top(0) {}
    void push(const U16& x) {
      if (top>=s.size()) error("IF or DO nested too deep");
      s[top++]=x;
    }
    U16 pop() {
      if (top<=0) error("unmatched IF or DO");
      return s[--top];
    }
  };

  Stack if_stack, do_stack;
};

//////////////////////// Compressor //////////////////////////

class Compressor {
public:
  Compressor(): enc(z), in(0), state(INIT), verify(false) {}
  void setOutput(Writer* out) {enc.out=out;}
  void writeTag();
  void startBlock(int level);  // level=1,2,3
  void startBlock(const char* hcomp);     // ZPAQL byte code
  void startBlock(const char* config,     // ZPAQL source code
                  int* args,              // NULL or int[9] arguments
                  Writer* pcomp_cmd = 0); // retrieve preprocessor command
  void setVerify(bool v) {verify = v;}    // check postprocessing?
  void hcomp(Writer* out2) {z.write(out2, false);}
  bool pcomp(Writer* out2) {return pz.write(out2, true);}
  void startSegment(const char* filename = 0, const char* comment = 0);
  void setInput(Reader* i) {in=i;}
  void postProcess(const char* pcomp = 0, int len = 0);  // byte code
  bool compress(int n = -1);  // n bytes, -1=all, return true until done
  void endSegment(const char* sha1string = 0);
  char* endSegmentChecksum(int64_t* size = 0);
  int64_t getSize() {return sha1.usize();}
  const char* getChecksum() {return sha1.result();}
  void endBlock();
  int stat(int x) {return enc.stat(x);}
private:
  ZPAQL z, pz;  // model and test postprocessor
  Encoder enc;  // arithmetic encoder containing predictor
  Reader* in;   // input source
  SHA1 sha1;    // to test pz output
  char sha1result[20];  // sha1 output
  enum {INIT, BLOCK1, SEG1, BLOCK2, SEG2} state;
  bool verify;  // if true then test by postprocessing
};

/////////////////////////// compress() ///////////////////////

void compress(Reader* in, Writer* out, int level);

}  // namespace libzpaq

#endif  // LIBZPAQ_H